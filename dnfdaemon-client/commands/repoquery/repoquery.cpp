/*
Copyright (C) 2020 Red Hat, Inc.

This file is part of dnfdaemon-client: https://github.com/rpm-software-management/libdnf/

Dnfdaemon-client is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Dnfdaemon-client is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with dnfdaemon-client.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "repoquery.hpp"

#include "../../context.hpp"

#include <dnfdaemon-server/dbus.hpp>
#include <libdnf/conf/option_string.hpp>
#include <libdnf/rpm/package.hpp>
#include <libdnf/rpm/package_set.hpp>
#include <libdnf/rpm/solv_query.hpp>

#include <iostream>

namespace dnfdaemon::client {

using namespace libdnf::cli;
void CmdRepoquery::set_argument_parser(Context & ctx) {
    available_option = dynamic_cast<libdnf::OptionBool *>(
        ctx.arg_parser.add_init_value(std::unique_ptr<libdnf::OptionBool>(new libdnf::OptionBool(true))));

    installed_option = dynamic_cast<libdnf::OptionBool *>(
        ctx.arg_parser.add_init_value(std::unique_ptr<libdnf::OptionBool>(new libdnf::OptionBool(false))));

    info_option = dynamic_cast<libdnf::OptionBool *>(
        ctx.arg_parser.add_init_value(std::unique_ptr<libdnf::OptionBool>(new libdnf::OptionBool(false))));

    nevra_option = dynamic_cast<libdnf::OptionBool *>(
        ctx.arg_parser.add_init_value(std::unique_ptr<libdnf::OptionBool>(new libdnf::OptionBool(true))));

    auto available = ctx.arg_parser.add_new_named_arg("available");
    available->set_long_name("available");
    available->set_short_description("display available packages (default)");
    available->set_const_value("true");
    available->link_value(available_option);

    auto installed = ctx.arg_parser.add_new_named_arg("installed");
    installed->set_long_name("installed");
    installed->set_short_description("display installed packages");
    installed->set_const_value("true");
    installed->link_value(installed_option);

    auto info = ctx.arg_parser.add_new_named_arg("info");
    info->set_long_name("info");
    info->set_short_description("show detailed information about the packages");
    info->set_const_value("true");
    info->link_value(info_option);

    auto nevra = ctx.arg_parser.add_new_named_arg("nevra");
    nevra->set_long_name("nevra");
    nevra->set_short_description(
        "use name-epoch:version-release.architecture format for displaying packages (default)");
    nevra->set_const_value("true");
    nevra->link_value(nevra_option);

    patterns_to_show_options = ctx.arg_parser.add_new_values();
    auto keys = ctx.arg_parser.add_new_positional_arg(
        "keys_to_match",
        ArgumentParser::PositionalArg::UNLIMITED,
        ctx.arg_parser.add_init_value(std::unique_ptr<libdnf::Option>(new libdnf::OptionString(nullptr))),
        patterns_to_show_options);
    keys->set_short_description("List of keys to match");

    auto conflict_args =
        ctx.arg_parser.add_conflict_args_group(std::unique_ptr<std::vector<ArgumentParser::Argument *>>(
            new std::vector<ArgumentParser::Argument *>{info, nevra}));

    info->set_conflict_arguments(conflict_args);
    nevra->set_conflict_arguments(conflict_args);

    auto repoquery = ctx.arg_parser.add_new_command("repoquery");
    repoquery->set_short_description("search for packages matching keyword");
    repoquery->set_description("");
    repoquery->set_named_args_help_header("Optional arguments:");
    repoquery->set_positional_args_help_header("Positional arguments:");
    repoquery->set_parse_hook_func([this, &ctx](
                                [[maybe_unused]] ArgumentParser::Argument * arg,
                                [[maybe_unused]] const char * option,
                                [[maybe_unused]] int argc,
                                [[maybe_unused]] const char * const argv[]) {
        ctx.select_command(this);
        return true;
    });

    repoquery->register_named_arg(available);
    repoquery->register_named_arg(installed);
    repoquery->register_named_arg(info);
    repoquery->register_named_arg(nevra);
    repoquery->register_positional_arg(keys);

    ctx.arg_parser.get_root_command()->register_command(repoquery);
}

class PackageDbus {
public:
    PackageDbus(dnfdaemon::KeyValueMap & rawdata) : rawdata(rawdata){};
    std::string get_full_nevra() { return rawdata["nevra"]; }

private:
    dnfdaemon::KeyValueMap rawdata;
};

void CmdRepoquery::run([[maybe_unused]] Context & ctx) {
    std::cout << "REPOQUERY Run" << std::endl;
    // prepare options from command line arguments
    dnfdaemon::KeyValueMap options = {};
    options["available"] = available_option->get_value();
    options["installed"] = installed_option->get_value();
    options["info"] = info_option->get_value();
    options["nevra"] = nevra_option->get_value();
    std::vector<std::string> patterns_to_show;
    if (patterns_to_show_options->size() > 0) {
        patterns_to_show.reserve(patterns_to_show_options->size());
        for (auto & pattern : *patterns_to_show_options) {
            patterns_to_show.emplace_back(dynamic_cast<libdnf::OptionString *>(pattern.get())->get_value());
        }
    }
    options["patterns_to_show"] = patterns_to_show;

    dnfdaemon::KeyValueMapList packages;
    ctx.session_proxy->callMethod("list")
        .onInterface(dnfdaemon::INTERFACE_RPM)
        .withArguments(options)
        .storeResultsTo(packages);

    for (auto & raw_package : packages) {
        PackageDbus package(raw_package);
        std::cout << "package: " << package.get_full_nevra() << std::endl;
    }
}

}  // namespace dnfdaemon::client
