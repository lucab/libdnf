/*
Copyright (C) 2020 Red Hat, Inc.

This file is part of dnfdaemon-server: https://github.com/rpm-software-management/libdnf/

Dnfdaemon-server is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Dnfdaemon-server is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with dnfdaemon-server.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "rpm.hpp"

#include "dnfdaemon-server/dbus.hpp"
#include "dnfdaemon-server/utils.hpp"

#include <fmt/format.h>
#include <libdnf/rpm/package_set.hpp>
#include <libdnf/rpm/repo.hpp>
#include <libdnf/rpm/solv_query.hpp>
#include <sdbus-c++/sdbus-c++.h>

#include <chrono>
#include <iostream>
#include <string>

void Rpm::dbus_register() {
    dbus_object = sdbus::createObject(session.get_connection(), session.get_object_path());
    dbus_object->registerMethod(
        dnfdaemon::INTERFACE_RPM, "list", "a{sv}", "aa{sv}", [this](sdbus::MethodCall call) -> void {
            this->list(std::move(call));
        });
    dbus_object->finishRegistration();
}

void Rpm::dbus_deregister() {
    dbus_object->unregister();
}

void Rpm::list(sdbus::MethodCall && call) {
    auto worker = std::thread([this](sdbus::MethodCall call) {
        try {
            // read options from dbus call
            dnfdaemon::KeyValueMap options;
            call >> options;
            std::vector<std::string> default_patterns{};
            std::vector<std::string> patterns_to_show =
                key_value_map_get<std::vector<std::string>>(options, "patterns_to_show", std::move(default_patterns));

            if (!session.read_all_repos(dbus_object)) {
                throw std::runtime_error("Cannot load repositories.");
            }

            auto & solv_sack = session.get_base()->get_rpm_solv_sack();
            libdnf::rpm::PackageSet result_pset(&solv_sack);
            libdnf::rpm::SolvQuery full_solv_query(&solv_sack);
            for (auto & pattern : patterns_to_show) {
                libdnf::rpm::SolvQuery solv_query(full_solv_query);
                solv_query.resolve_pkg_spec(pattern, true, true, true, true, true, {});
                result_pset |= solv_query.get_package_set();
            }

            // create reply from the query
            dnfdaemon::KeyValueMapList out_packages;
            for (auto pkg : result_pset) {
                dnfdaemon::KeyValueMap package;
                package.emplace(std::make_pair("nevra", pkg.get_full_nevra()));
                std::cout << "XXXXX nevra: " << pkg.get_full_nevra() << std::endl;
                out_packages.push_back(std::move(package));
            }

            auto reply = call.createReply();
            reply << out_packages;
            reply.send();
        } catch (std::exception & ex) {
            DNFDAEMON_ERROR_REPLY(call, ex);
        }
        session.get_threads_manager().current_thread_finished();
    }, std::move(call));
    session.get_threads_manager().register_thread(std::move(worker));
}
