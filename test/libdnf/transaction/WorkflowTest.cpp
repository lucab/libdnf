#include <cstdio>
#include <iostream>
#include <string>

#include "libdnf/transaction/CompsEnvironmentItem.hpp"
#include "libdnf/transaction/comps_group.hpp"
#include "libdnf/transaction/RPMItem.hpp"
#include "libdnf/transaction/transaction.hpp"
#include "libdnf/transaction/transaction_item.hpp"
#include "libdnf/transaction/Transformer.hpp"

#include "WorkflowTest.hpp"

using namespace libdnf::transaction;

CPPUNIT_TEST_SUITE_REGISTRATION(WorkflowTest);

void
WorkflowTest::setUp()
{
    conn = new libdnf::utils::SQLite3(":memory:");
    Transformer::createDatabase(*conn);
}

void
WorkflowTest::tearDown()
{
    delete conn;
}

void
WorkflowTest::testDefaultWorkflow()
{
    // TODO: init/begin/end trans

    // STEP 1: create transaction object
    Transaction trans(*conn);
    CPPUNIT_ASSERT_EQUAL(TransactionState::UNKNOWN, trans.get_state());

    // STEP 2: set vars
    trans.set_releasever("26");

    // populate goal
    // resolve dependencies
    // prepare RPM transaction

    // STEP 3: associate RPMs to the transaction
    // bash-4.4.12-5.fc26.x86_64
    auto rpm_bash = std::make_shared< RPMItem >(trans);
    rpm_bash->setName("bash");
    rpm_bash->setEpoch(0);
    rpm_bash->setVersion("4.4.12");
    rpm_bash->setRelease("5.fc26");
    rpm_bash->setArch("x86_64");
    std::string repoid = "base";
    TransactionItemAction action = TransactionItemAction::INSTALL;
    TransactionItemReason reason = TransactionItemReason::GROUP;
    trans.addItem(rpm_bash, repoid, action, reason);

    // systemd-233-6.fc26
    auto rpm_systemd = std::make_shared< RPMItem >(trans);
    rpm_systemd->setName("systemd");
    rpm_systemd->setEpoch(0);
    rpm_systemd->setVersion("233");
    rpm_systemd->setRelease("6.fc26");
    rpm_systemd->setArch("x86_64");
    repoid = "base";
    action = TransactionItemAction::OBSOLETE;
    reason = TransactionItemReason::USER;
    auto ti_rpm_systemd = trans.addItem(rpm_systemd, repoid, action, reason);

    // sysvinit-2.88-14.dsf.fc20
    auto rpm_sysvinit = std::make_shared< RPMItem >(trans);
    rpm_sysvinit->setName("sysvinit");
    rpm_sysvinit->setEpoch(0);
    rpm_sysvinit->setVersion("2.88");
    rpm_sysvinit->setRelease("14.dsf.fc20");
    rpm_sysvinit->setArch("x86_64");
    repoid = "f20";
    action = TransactionItemAction::OBSOLETED;
    reason = TransactionItemReason::USER;
    auto ti_rpm_sysvinit = trans.addItem(rpm_sysvinit, repoid, action, reason);
    ti_rpm_sysvinit->addReplacedBy(ti_rpm_systemd);

    auto comps_group_core = std::make_shared< CompsGroup >(trans);
    comps_group_core->set_group_id("core");
    comps_group_core->set_name("Core");
    comps_group_core->set_translated_name("Úplný základ");
    comps_group_core->add_package("bash", true, CompsPackageType::MANDATORY);
    repoid = "";
    action = TransactionItemAction::INSTALL;
    reason = TransactionItemReason::USER;
    trans.addItem(comps_group_core, repoid, action, reason);

    auto comps_environment_minimal = std::make_shared< CompsEnvironmentItem >(trans);
    comps_environment_minimal->setEnvironmentId("minimal");
    comps_environment_minimal->setName("Minimal");
    comps_environment_minimal->setTranslatedName("mmm");
    comps_environment_minimal->addGroup("core", true, CompsPackageType::MANDATORY);
    repoid = "";
    action = TransactionItemAction::INSTALL;
    reason = TransactionItemReason::USER;
    trans.addItem(comps_environment_minimal, repoid, action, reason);

    // STEP 4: save transaction and all associated items
    trans.begin();

    // STEP 5: run RPM transaction; callback: mark completed items
    for (auto i : trans.getItems()) {
        i->set_state(TransactionItemState::DONE);
        i->save();
    }

    // STEP 6
    // mark completed transaction
    trans.finish(TransactionState::DONE);
    CPPUNIT_ASSERT_EQUAL(TransactionState::DONE, trans.get_state());

    // VERIFY
    // verify that data is available via public API
    auto trans2 = Transaction(*conn, trans.get_id());
    CPPUNIT_ASSERT_EQUAL(TransactionState::DONE, trans2.get_state());

    CPPUNIT_ASSERT(trans2.getItems().size() == 5);

    for (auto i : trans2.getItems()) {
        if (i->get_id() == 1) {
            CPPUNIT_ASSERT(i->get_action() == TransactionItemAction::INSTALL);
            CPPUNIT_ASSERT(i->get_reason() == TransactionItemReason::GROUP);
            CPPUNIT_ASSERT(i->get_repoid() == "base");
        } else if (i->get_id() == 2) {
            CPPUNIT_ASSERT(i->get_action() == TransactionItemAction::OBSOLETE);
            CPPUNIT_ASSERT(i->get_reason() == TransactionItemReason::USER);
            CPPUNIT_ASSERT(i->get_repoid() == "base");
        } else if (i->get_id() == 3) {
            CPPUNIT_ASSERT(i->get_action() == TransactionItemAction::OBSOLETED);
            CPPUNIT_ASSERT(i->get_reason() == TransactionItemReason::USER);
            CPPUNIT_ASSERT(i->get_repoid() == "f20");
        }

        // CPPUNIT_ASSERT(i->getItem()->getItemType() == "rpm");
        CPPUNIT_ASSERT_EQUAL(TransactionItemState::DONE, i->get_state());
        // std::cout << "TransactionItem: " << i->getItem()->toStr() << std::endl;
        if (i->getItem()->getItemType() == TransactionItemType::GROUP) {
            auto grp = std::dynamic_pointer_cast< CompsGroup >(i->getItem());
            CPPUNIT_ASSERT(grp->getPackages().size() == 1);
            for (auto i : grp->getPackages()) {
                // std::cout << "  CompsGroupPackage: " << i->getName() << std::endl;
            }
        }
        if (i->getItem()->getItemType() == TransactionItemType::ENVIRONMENT) {
            auto env = std::dynamic_pointer_cast< CompsEnvironmentItem >(i->getItem());
            CPPUNIT_ASSERT(env->getGroups().size() == 1);
            for (auto i : env->getGroups()) {
                // std::cout << "  CompsEnvironmentGroup: @" << i->getGroupId() << std::endl;
            }
        }
    }
}
