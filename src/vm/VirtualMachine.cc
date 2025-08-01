/* -------------------------------------------------------------------------- */
/* Copyright 2002-2025, OpenNebula Project, OpenNebula Systems                */
/*                                                                            */
/* Licensed under the Apache License, Version 2.0 (the "License"); you may    */
/* not use this file except in compliance with the License. You may obtain    */
/* a copy of the License at                                                   */
/*                                                                            */
/* http://www.apache.org/licenses/LICENSE-2.0                                 */
/*                                                                            */
/* Unless required by applicable law or agreed to in writing, software        */
/* distributed under the License is distributed on an "AS IS" BASIS,          */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   */
/* See the License for the specific language governing permissions and        */
/* limitations under the License.                                             */
/* -------------------------------------------------------------------------- */
#include "VirtualMachine.h"
#include "VirtualMachineManager.h"
#include "VirtualNetworkPool.h"
#include "VMGroupPool.h"
#include "ImagePool.h"
#include "NebulaLog.h"
#include "NebulaUtil.h"
#include "Snapshots.h"
#include "LifeCycleManager.h"
#include "ClusterPool.h"
#include "DatastorePool.h"
#include "SecurityGroupPool.h"
#include "Nebula.h"
#include "ScheduledActionPool.h"

#include "vm_file_var_syntax.h"
#include "vm_var_syntax.h"

#include <sys/stat.h>
#include <regex>

using namespace std;

/* ************************************************************************** */
/* Virtual Machine :: Constructor/Destructor                                  */
/* ************************************************************************** */

VirtualMachine::VirtualMachine(int           id,
                               int           _uid,
                               int           _gid,
                               const string& _uname,
                               const string& _gname,
                               int           umask,
                               unique_ptr<VirtualMachineTemplate> _vm_template):
    PoolObjectSQL(id, VM, "", _uid, _gid, _uname, _gname, one_db::vm_table),
    state(INIT),
    prev_state(INIT),
    lcm_state(LCM_INIT),
    prev_lcm_state(LCM_INIT),
    resched(0),
    stime(time(0)),
    etime(0),
    deploy_id(""),
    disks(false),
    nics(false),
    _log(0),
    _sched_actions("SCHED_ACTIONS")
{
    if (_vm_template != 0)
    {
        // This is a VM Template, with the root TEMPLATE.
        _vm_template->set_xml_root("USER_TEMPLATE");

        user_obj_template = move(_vm_template);
    }
    else
    {
        user_obj_template = make_unique<VirtualMachineTemplate>(false, '=', "USER_TEMPLATE");
    }

    obj_template = make_unique<VirtualMachineTemplate>();

    set_umask(umask);
}

VirtualMachine::~VirtualMachine()
{
    delete _log;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::vm_state_from_str(string& st, VmState& state)
{
    one_util::toupper(st);

    if ( st == "INIT" )
    {
        state = INIT;
    }
    else if ( st == "PENDING" )
    {
        state = PENDING;
    }
    else if ( st == "HOLD" )
    {
        state = HOLD;
    }
    else if ( st == "ACTIVE" )
    {
        state = ACTIVE;
    }
    else if ( st == "STOPPED" )
    {
        state = STOPPED;
    }
    else if ( st == "SUSPENDED" )
    {
        state = SUSPENDED;
    }
    else if ( st == "DONE" )
    {
        state = DONE;
    }
    else if ( st == "POWEROFF" )
    {
        state = POWEROFF;
    }
    else if ( st == "UNDEPLOYED" )
    {
        state = UNDEPLOYED;
    }
    else if ( st == "CLONING" )
    {
        state = CLONING;
    }
    else if ( st == "CLONING_FAILURE" )
    {
        state = CLONING_FAILURE;
    }
    else
    {
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

string& VirtualMachine::vm_state_to_str(string& st, VmState state)
{
    switch (state)
    {
        case INIT:
            st = "INIT"; break;
        case PENDING:
            st = "PENDING"; break;
        case HOLD:
            st = "HOLD"; break;
        case ACTIVE:
            st = "ACTIVE"; break;
        case STOPPED:
            st = "STOPPED"; break;
        case SUSPENDED:
            st = "SUSPENDED"; break;
        case DONE:
            st = "DONE"; break;
        case POWEROFF:
            st = "POWEROFF"; break;
        case UNDEPLOYED:
            st = "UNDEPLOYED"; break;
        case CLONING:
            st = "CLONING"; break;
        case CLONING_FAILURE:
            st = "CLONING_FAILURE"; break;
    }

    return st;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::lcm_state_from_str(string& st, LcmState& state)
{
    one_util::toupper(st);

    if ( st == "LCM_INIT" )
    {
        state = LCM_INIT;
    }
    else if ( st == "PROLOG" )
    {
        state = PROLOG;
    }
    else if ( st == "BOOT" )
    {
        state = BOOT;
    }
    else if ( st == "RUNNING" )
    {
        state = RUNNING;
    }
    else if ( st == "MIGRATE" )
    {
        state = MIGRATE;
    }
    else if ( st == "SAVE_STOP" )
    {
        state = SAVE_STOP;
    }
    else if ( st == "SAVE_SUSPEND" )
    {
        state = SAVE_SUSPEND;
    }
    else if ( st == "SAVE_MIGRATE" )
    {
        state = SAVE_MIGRATE;
    }
    else if ( st == "PROLOG_MIGRATE" )
    {
        state = PROLOG_MIGRATE;
    }
    else if ( st == "PROLOG_RESUME" )
    {
        state = PROLOG_RESUME;
    }
    else if ( st == "EPILOG_STOP" )
    {
        state = EPILOG_STOP;
    }
    else if ( st == "EPILOG" )
    {
        state = EPILOG;
    }
    else if ( st == "SHUTDOWN" )
    {
        state = SHUTDOWN;
    }
    else if ( st == "CLEANUP_RESUBMIT" )
    {
        state = CLEANUP_RESUBMIT;
    }
    else if ( st == "UNKNOWN" )
    {
        state = UNKNOWN;
    }
    else if ( st == "HOTPLUG" )
    {
        state = HOTPLUG;
    }
    else if ( st == "SHUTDOWN_POWEROFF" )
    {
        state = SHUTDOWN_POWEROFF;
    }
    else if ( st == "BOOT_UNKNOWN" )
    {
        state = BOOT_UNKNOWN;
    }
    else if ( st == "BOOT_POWEROFF" )
    {
        state = BOOT_POWEROFF;
    }
    else if ( st == "BOOT_SUSPENDED" )
    {
        state = BOOT_SUSPENDED;
    }
    else if ( st == "BOOT_STOPPED" )
    {
        state = BOOT_STOPPED;
    }
    else if ( st == "CLEANUP_DELETE" )
    {
        state = CLEANUP_DELETE;
    }
    else if ( st == "HOTPLUG_SNAPSHOT" )
    {
        state = HOTPLUG_SNAPSHOT;
    }
    else if ( st == "HOTPLUG_NIC" )
    {
        state = HOTPLUG_NIC;
    }
    else if ( st == "HOTPLUG_SAVEAS" )
    {
        state = HOTPLUG_SAVEAS;
    }
    else if ( st == "HOTPLUG_SAVEAS_POWEROFF" )
    {
        state = HOTPLUG_SAVEAS_POWEROFF;
    }
    else if ( st == "HOTPLUG_SAVEAS_SUSPENDED" )
    {
        state = HOTPLUG_SAVEAS_SUSPENDED;
    }
    else if ( st == "SHUTDOWN_UNDEPLOY" )
    {
        state = SHUTDOWN_UNDEPLOY;
    }
    else if ( st == "EPILOG_UNDEPLOY" )
    {
        state = EPILOG_UNDEPLOY;
    }
    else if ( st == "PROLOG_UNDEPLOY" )
    {
        state = PROLOG_UNDEPLOY;
    }
    else if ( st == "BOOT_UNDEPLOY" )
    {
        state = BOOT_UNDEPLOY;
    }
    else if ( st == "HOTPLUG_PROLOG_POWEROFF" )
    {
        state = HOTPLUG_PROLOG_POWEROFF;
    }
    else if ( st == "HOTPLUG_EPILOG_POWEROFF" )
    {
        state = HOTPLUG_EPILOG_POWEROFF;
    }
    else if ( st == "BOOT_MIGRATE" )
    {
        state = BOOT_MIGRATE;
    }
    else if ( st == "BOOT_FAILURE" )
    {
        state = BOOT_FAILURE;
    }
    else if ( st == "BOOT_MIGRATE_FAILURE" )
    {
        state = BOOT_MIGRATE_FAILURE;
    }
    else if ( st == "PROLOG_MIGRATE_FAILURE" )
    {
        state = PROLOG_MIGRATE_FAILURE;
    }
    else if ( st == "PROLOG_FAILURE" )
    {
        state = PROLOG_FAILURE;
    }
    else if ( st == "EPILOG_FAILURE" )
    {
        state = EPILOG_FAILURE;
    }
    else if ( st == "EPILOG_STOP_FAILURE" )
    {
        state = EPILOG_STOP_FAILURE;
    }
    else if ( st == "EPILOG_UNDEPLOY_FAILURE" )
    {
        state = EPILOG_UNDEPLOY_FAILURE;
    }
    else if ( st == "PROLOG_MIGRATE_POWEROFF" )
    {
        state = PROLOG_MIGRATE_POWEROFF;
    }
    else if ( st == "PROLOG_MIGRATE_POWEROFF_FAILURE" )
    {
        state = PROLOG_MIGRATE_POWEROFF_FAILURE;
    }
    else if ( st == "PROLOG_MIGRATE_SUSPEND" )
    {
        state = PROLOG_MIGRATE_SUSPEND;
    }
    else if ( st == "PROLOG_MIGRATE_SUSPEND_FAILURE" )
    {
        state = PROLOG_MIGRATE_SUSPEND_FAILURE;
    }
    else if ( st == "BOOT_STOPPED_FAILURE" )
    {
        state = BOOT_STOPPED_FAILURE;
    }
    else if ( st == "BOOT_UNDEPLOY_FAILURE" )
    {
        state = BOOT_UNDEPLOY_FAILURE;
    }
    else if ( st == "PROLOG_RESUME_FAILURE" )
    {
        state = PROLOG_RESUME_FAILURE;
    }
    else if ( st == "PROLOG_UNDEPLOY_FAILURE" )
    {
        state = PROLOG_UNDEPLOY_FAILURE;
    }
    else if ( st == "DISK_SNAPSHOT_POWEROFF" )
    {
        state = DISK_SNAPSHOT_POWEROFF;
    }
    else if ( st == "DISK_SNAPSHOT_REVERT_POWEROFF" )
    {
        state = DISK_SNAPSHOT_REVERT_POWEROFF;
    }
    else if ( st == "DISK_SNAPSHOT_DELETE_POWEROFF" )
    {
        state = DISK_SNAPSHOT_DELETE_POWEROFF;
    }
    else if ( st == "DISK_SNAPSHOT_SUSPENDED" )
    {
        state = DISK_SNAPSHOT_SUSPENDED;
    }
    else if ( st == "DISK_SNAPSHOT_DELETE_SUSPENDED" )
    {
        state = DISK_SNAPSHOT_DELETE_SUSPENDED;
    }
    else if ( st == "DISK_SNAPSHOT" )
    {
        state = DISK_SNAPSHOT;
    }
    else if ( st == "DISK_SNAPSHOT_DELETE" )
    {
        state = DISK_SNAPSHOT_DELETE;
    }
    else if ( st == "PROLOG_MIGRATE_UNKNOWN" )
    {
        state = PROLOG_MIGRATE_UNKNOWN;
    }
    else if ( st == "PROLOG_MIGRATE_UNKNOWN_FAILURE" )
    {
        state = PROLOG_MIGRATE_UNKNOWN_FAILURE;
    }
    else if ( st == "DISK_RESIZE" )
    {
        state = DISK_RESIZE;
    }
    else if ( st == "DISK_RESIZE_POWEROFF" )
    {
        state = DISK_RESIZE_POWEROFF;
    }
    else if ( st == "DISK_RESIZE_UNDEPLOYED" )
    {
        state = DISK_RESIZE_UNDEPLOYED;
    }
    else if ( st == "HOTPLUG_NIC_POWEROFF" )
    {
        state = HOTPLUG_NIC_POWEROFF;
    }
    else if ( st == "HOTPLUG_RESIZE" )
    {
        state = HOTPLUG_RESIZE;
    }
    else if ( st == "HOTPLUG_SAVEAS_UNDEPLOYED" )
    {
        state = HOTPLUG_SAVEAS_UNDEPLOYED;
    }
    else if ( st == "HOTPLUG_SAVEAS_STOPPED" )
    {
        state = HOTPLUG_SAVEAS_STOPPED;
    }
    else if ( st == "BACKUP" )
    {
        state = BACKUP;
    }
    else if ( st == "BACKUP_POWEROFF" )
    {
        state = BACKUP_POWEROFF;
    }
    else if ( st == "RESTORE" )
    {
        state = RESTORE;
    }
    else
    {
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

string& VirtualMachine::lcm_state_to_str(string& st, LcmState state)
{
    switch (state)
    {
        case LCM_INIT:
            st = "LCM_INIT"; break;
        case PROLOG:
            st = "PROLOG"; break;
        case BOOT:
            st = "BOOT"; break;
        case RUNNING:
            st = "RUNNING"; break;
        case MIGRATE:
            st = "MIGRATE"; break;
        case SAVE_STOP:
            st = "SAVE_STOP"; break;
        case SAVE_SUSPEND:
            st = "SAVE_SUSPEND"; break;
        case SAVE_MIGRATE:
            st = "SAVE_MIGRATE"; break;
        case PROLOG_MIGRATE:
            st = "PROLOG_MIGRATE"; break;
        case PROLOG_RESUME:
            st = "PROLOG_RESUME"; break;
        case EPILOG_STOP:
            st = "EPILOG_STOP"; break;
        case EPILOG:
            st = "EPILOG"; break;
        case SHUTDOWN:
            st = "SHUTDOWN"; break;
        case CLEANUP_RESUBMIT:
            st = "CLEANUP_RESUBMIT"; break;
        case UNKNOWN:
            st = "UNKNOWN"; break;
        case HOTPLUG:
            st = "HOTPLUG"; break;
        case SHUTDOWN_POWEROFF:
            st = "SHUTDOWN_POWEROFF"; break;
        case BOOT_UNKNOWN:
            st = "BOOT_UNKNOWN"; break;
        case BOOT_POWEROFF:
            st = "BOOT_POWEROFF"; break;
        case BOOT_SUSPENDED:
            st = "BOOT_SUSPENDED"; break;
        case BOOT_STOPPED:
            st = "BOOT_STOPPED"; break;
        case CLEANUP_DELETE:
            st = "CLEANUP_DELETE"; break;
        case HOTPLUG_SNAPSHOT:
            st = "HOTPLUG_SNAPSHOT"; break;
        case HOTPLUG_NIC:
            st = "HOTPLUG_NIC"; break;
        case HOTPLUG_SAVEAS:
            st = "HOTPLUG_SAVEAS"; break;
        case HOTPLUG_SAVEAS_POWEROFF:
            st = "HOTPLUG_SAVEAS_POWEROFF"; break;
        case HOTPLUG_SAVEAS_SUSPENDED:
            st = "HOTPLUG_SAVEAS_SUSPENDED"; break;
        case SHUTDOWN_UNDEPLOY:
            st = "SHUTDOWN_UNDEPLOY"; break;
        case EPILOG_UNDEPLOY:
            st = "EPILOG_UNDEPLOY"; break;
        case PROLOG_UNDEPLOY:
            st = "PROLOG_UNDEPLOY"; break;
        case BOOT_UNDEPLOY:
            st = "BOOT_UNDEPLOY"; break;
        case HOTPLUG_PROLOG_POWEROFF:
            st = "HOTPLUG_PROLOG_POWEROFF"; break;
        case HOTPLUG_EPILOG_POWEROFF:
            st = "HOTPLUG_EPILOG_POWEROFF"; break;
        case BOOT_MIGRATE:
            st = "BOOT_MIGRATE"; break;
        case BOOT_FAILURE:
            st = "BOOT_FAILURE"; break;
        case BOOT_MIGRATE_FAILURE:
            st = "BOOT_MIGRATE_FAILURE"; break;
        case PROLOG_MIGRATE_FAILURE:
            st = "PROLOG_MIGRATE_FAILURE"; break;
        case PROLOG_FAILURE:
            st = "PROLOG_FAILURE"; break;
        case EPILOG_FAILURE:
            st = "EPILOG_FAILURE"; break;
        case EPILOG_STOP_FAILURE:
            st = "EPILOG_STOP_FAILURE"; break;
        case EPILOG_UNDEPLOY_FAILURE:
            st = "EPILOG_UNDEPLOY_FAILURE"; break;
        case PROLOG_MIGRATE_POWEROFF:
            st = "PROLOG_MIGRATE_POWEROFF"; break;
        case PROLOG_MIGRATE_POWEROFF_FAILURE:
            st = "PROLOG_MIGRATE_POWEROFF_FAILURE"; break;
        case PROLOG_MIGRATE_SUSPEND:
            st = "PROLOG_MIGRATE_SUSPEND"; break;
        case PROLOG_MIGRATE_SUSPEND_FAILURE:
            st = "PROLOG_MIGRATE_SUSPEND_FAILURE"; break;
        case BOOT_STOPPED_FAILURE:
            st = "BOOT_STOPPED_FAILURE"; break;
        case BOOT_UNDEPLOY_FAILURE:
            st = "BOOT_UNDEPLOY_FAILURE"; break;
        case PROLOG_RESUME_FAILURE:
            st = "PROLOG_RESUME_FAILURE"; break;
        case PROLOG_UNDEPLOY_FAILURE:
            st = "PROLOG_UNDEPLOY_FAILURE"; break;
        case DISK_SNAPSHOT_POWEROFF:
            st = "DISK_SNAPSHOT_POWEROFF"; break;
        case DISK_SNAPSHOT_REVERT_POWEROFF:
            st = "DISK_SNAPSHOT_REVERT_POWEROFF"; break;
        case DISK_SNAPSHOT_DELETE_POWEROFF:
            st = "DISK_SNAPSHOT_DELETE_POWEROFF"; break;
        case DISK_SNAPSHOT_SUSPENDED:
            st = "DISK_SNAPSHOT_SUSPENDED"; break;
        case DISK_SNAPSHOT_DELETE_SUSPENDED:
            st = "DISK_SNAPSHOT_DELETE_SUSPENDED"; break;
        case DISK_SNAPSHOT:
            st = "DISK_SNAPSHOT"; break;
        case DISK_SNAPSHOT_DELETE:
            st = "DISK_SNAPSHOT_DELETE"; break;
        case PROLOG_MIGRATE_UNKNOWN:
            st = "PROLOG_MIGRATE_UNKNOWN"; break;
        case PROLOG_MIGRATE_UNKNOWN_FAILURE:
            st = "PROLOG_MIGRATE_UNKNOWN_FAILURE"; break;
        case DISK_RESIZE:
            st = "DISK_RESIZE"; break;
        case DISK_RESIZE_POWEROFF:
            st = "DISK_RESIZE_POWEROFF"; break;
        case DISK_RESIZE_UNDEPLOYED:
            st = "DISK_RESIZE_UNDEPLOYED"; break;
        case HOTPLUG_NIC_POWEROFF:
            st = "HOTPLUG_NIC_POWEROFF"; break;
        case HOTPLUG_RESIZE:
            st = "HOTPLUG_RESIZE"; break;
        case HOTPLUG_SAVEAS_UNDEPLOYED:
            st = "HOTPLUG_SAVEAS_UNDEPLOYED"; break;
        case HOTPLUG_SAVEAS_STOPPED:
            st = "HOTPLUG_SAVEAS_STOPPED"; break;
        case BACKUP:
            st = "BACKUP"; break;
        case BACKUP_POWEROFF:
            st = "BACKUP_POWEROFF"; break;
        case RESTORE:
            st = "RESTORE"; break;
    }

    return st;
}

/* -------------------------------------------------------------------------- */

string VirtualMachine::state_str()
{
    string st;

    if (state == ACTIVE)
    {
        return lcm_state_to_str(st, lcm_state);
    }

    return vm_state_to_str(st, state);
}

/* ************************************************************************** */
/* Virtual Machine :: Database Access Functions                               */
/* ************************************************************************** */

int VirtualMachine::bootstrap(SqlDB * db)
{
    int rc;

    ostringstream oss_vm;

    oss_vm << one_db::vm_db_bootstrap;

    ostringstream oss_monit(one_db::vm_monitor_db_bootstrap);
    ostringstream oss_hist(one_db::history_db_bootstrap);
    ostringstream oss_showback(one_db::vm_showback_db_bootstrap);

    ostringstream oss_index("CREATE INDEX state_oid_idx on vm_pool (state, oid);");

    rc =  db->exec_local_wr(oss_vm);
    rc += db->exec_local_wr(oss_index);

    rc += db->exec_local_wr(oss_monit);
    rc += db->exec_local_wr(oss_hist);
    rc += db->exec_local_wr(oss_showback);

    return rc;
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::select(SqlDB * db)
{

    ostringstream   oss;
    ostringstream   ose;

    int    rc;
    int    seq;

    Nebula& nd = Nebula::instance();

    // Rebuild the VirtualMachine object
    rc = PoolObjectSQL::select(db);

    if ( rc != 0 )
    {
        return rc;
    }

    if (history && history->select(db) != 0)
    {
        seq = history->seq;
        goto error_common;
    }
    if (previous_history && previous_history->select(db) != 0)
    {
        seq = previous_history->seq;
        goto error_common;
    }

    if ( state == DONE ) //Do not recreate dirs. They may be deleted
    {
        _log = 0;

        return 0;
    }

    //--------------------------------------------------------------------------
    //Create support directories for this VM
    //--------------------------------------------------------------------------
    oss.str("");
    oss << nd.get_vms_location() << oid;

    mkdir(oss.str().c_str(), 0700);
    chmod(oss.str().c_str(), 0700);

    //--------------------------------------------------------------------------
    //Create Log support for this VM
    //--------------------------------------------------------------------------
    try
    {
        Log::MessageType   clevel;
        NebulaLog::LogType log_system;

        log_system  = nd.get_log_system();
        clevel      = nd.get_debug_level();

        switch (log_system)
        {
            case NebulaLog::FILE_TS:
            case NebulaLog::FILE:
                _log = new FileLog(nd.get_vm_log_filename(oid), clevel);
                break;

            case NebulaLog::SYSLOG:
                _log = new SysLog(clevel, oid, obj_type);
                break;

            case NebulaLog::STD:
                _log = new StdLog(clevel, oid, obj_type);
                break;

            default:
                throw runtime_error("Unknown log system.");
                break;
        }
    }
    catch(exception &e)
    {
        ose << "Error creating log: " << e.what();
        NebulaLog::log("ONE", Log::ERROR, ose);

        _log = 0;
    }

    return 0;

error_common:
    ose << "Error loading history for VM " << oid  << " seq " << seq;

    log("ONE", Log::ERROR, ose);
    NebulaLog::error("ONE", ose.str());

    return -1;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

static int set_boot_order(Template * tmpl, string& error_str)
{
    vector<VectorAttribute *> disk;
    vector<VectorAttribute *> nic;

    ostringstream oss;

    int ndisk = tmpl->get("DISK", disk);
    int nnic  = tmpl->get("NIC", nic);

    for (int i=0; i<ndisk; ++i)
    {
        disk[i]->remove("ORDER");
    }

    for (int i=0; i<nnic; ++i)
    {
        nic[i]->remove("ORDER");
    }

    VectorAttribute * os = tmpl->get("OS");

    if ( os == 0 )
    {
        return 0;
    }

    string order = os->vector_value("BOOT");

    if ( order.empty() )
    {
        return 0;
    }

    vector<string> bdevs = one_util::split(order, ',');

    int index = 1;

    for (auto& str_dev : bdevs)
    {
        vector<VectorAttribute *> * dev;
        int    max;
        int    disk_id;
        size_t pos;

        const char * id_name;

        one_util::toupper(str_dev);

        int rc = one_util::regex_match("^(DISK|NIC)[[:digit:]]+$", str_dev.c_str());

        if (rc != 0)
        {
            goto error_parsing;
        }

        if (str_dev.compare(0, 4, "DISK") == 0)
        {
            pos = 4;

            max = ndisk;
            dev = &disk;

            id_name = "DISK_ID";
        }
        else if (str_dev.compare(0, 3, "NIC") == 0)
        {
            pos = 3;

            max = nnic;
            dev = &nic;

            id_name = "NIC_ID";
        }
        else
        {
            goto error_parsing;
        }

        istringstream iss(str_dev.substr(pos, string::npos));

        iss >> disk_id;

        if (iss.fail())
        {
            goto error_parsing;
        }

        bool found = false;

        for (int j=0; j<max; ++j)
        {
            int j_disk_id;

            if ( (*dev)[j]->vector_value(id_name, j_disk_id) == 0 &&
                 j_disk_id == disk_id )
            {
                (*dev)[j]->replace("ORDER", index++);
                found = true;
            }
        }

        if (!found)
        {
            oss << "Wrong OS/BOOT value. Device with "
                << id_name << " " << disk_id << " not found";

            goto error_common;
        }
    }

    return 0;

error_parsing:
    oss << "Wrong OS/BOOT value: \"" << order
        << "\" should be a comma-separated list of disk# or nic#";

error_common:
    error_str = oss.str();
    return -1;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

static int parse_memory_mode(string& mem_mode, string& error);

int VirtualMachine::insert(SqlDB * db, string& error_str)
{
    int    rc;
    string name;
    string prefix;

    string value;
    long int ivalue;
    long int memory;
    float fvalue;
    set<int> cluster_ids;
    vector<Template *> quotas;
    ostringstream oss;

    //Decrypt attributes before parsing them
    decrypt();

    // ------------------------------------------------------------------------
    // Set a name if the VM has not got one and VM_ID
    // ------------------------------------------------------------------------
    user_obj_template->erase("VMID");
    obj_template->add("VMID", oid);

    user_obj_template->get("TEMPLATE_ID", value);
    user_obj_template->erase("TEMPLATE_ID");

    if (!value.empty())
    {
        obj_template->add("TEMPLATE_ID", value);
    }

    user_obj_template->get("NAME", name);
    user_obj_template->erase("NAME");

    user_obj_template->get("TEMPLATE_NAME", prefix);
    user_obj_template->erase("TEMPLATE_NAME");

    if (prefix.empty())
    {
        prefix = "one";
    }

    if (name.empty() == true)
    {
        oss.str("");
        oss << prefix << "-" << oid;
        name = oss.str();
    }

    if ( !PoolObjectSQL::name_is_valid(name, error_str) )
    {
        goto error_name;
    }

    this->name = name;

    // -------------------------------------------------------------------------
    // Move known attributes that doesn't need additional procesing to template
    // -------------------------------------------------------------------------
    parse_well_known_attributes();

    // ------------------------------------------------------------------------
    // Check for EMULATOR attribute
    // ------------------------------------------------------------------------
    user_obj_template->get("EMULATOR", value);

    if (!value.empty())
    {
        user_obj_template->erase("EMULATOR");
        obj_template->add("EMULATOR", value);
    }

    // ------------------------------------------------------------------------
    // Check for CPU, VCPU, MEMORY and TOPOLOGY attributes
    // ------------------------------------------------------------------------
    if ( user_obj_template->get("MEMORY", memory) == false || memory <= 0 )
    {
        goto error_memory;
    }

    user_obj_template->erase("MEMORY");
    obj_template->add("MEMORY", memory);

    // Check optional MEMORY attributes
    if ( user_obj_template->get("MEMORY_RESIZE_MODE", value) )
    {
        if ( parse_memory_mode(value, error_str) == -1 )
        {
            goto error_mem_mode;
        }

        user_obj_template->erase("MEMORY_RESIZE_MODE");
        obj_template->add("MEMORY_RESIZE_MODE", value);
    }

    if ( user_obj_template->get("MEMORY_MAX", ivalue) && ivalue > 0 )
    {
        user_obj_template->erase("MEMORY_MAX");
        obj_template->add("MEMORY_MAX", ivalue);
    }

    if ( user_obj_template->get("MEMORY_SLOTS", ivalue) && ivalue > 0 )
    {
        user_obj_template->erase("MEMORY_SLOTS");
        obj_template->add("MEMORY_SLOTS", ivalue);
    }

    if ( user_obj_template->get("CPU", fvalue) == false || fvalue <= 0 )
    {
        goto error_cpu;
    }

    user_obj_template->erase("CPU");
    obj_template->add("CPU", fvalue);

    // VCPU is optional, first check if the attribute exists, then check it is
    // an integer
    user_obj_template->get("VCPU", value);

    if ( value.empty() == false )
    {
        if ( user_obj_template->get("VCPU", ivalue) == false || ivalue <= 0 )
        {
            goto error_vcpu;
        }

        user_obj_template->erase("VCPU");
        obj_template->add("VCPU", ivalue);
    }

    // Check optional VCPU_MAX attribute
    if ( user_obj_template->get("VCPU_MAX", ivalue) && ivalue > 0 )
    {
        user_obj_template->erase("VCPU_MAX");
        obj_template->add("VCPU_MAX", ivalue);
    }

    // ------------------------------------------------------------------------
    // Check the cost attributes
    // ------------------------------------------------------------------------
    if ( user_obj_template->get("CPU_COST", fvalue) == true )
    {
        if ( fvalue < 0 )
        {
            goto error_cpu_cost;
        }

        user_obj_template->erase("CPU_COST");
        obj_template->add("CPU_COST", fvalue);
    }

    if ( user_obj_template->get("MEMORY_COST", fvalue) == true )
    {
        if ( fvalue < 0 )
        {
            goto error_memory_cost;
        }

        user_obj_template->erase("MEMORY_COST");
        obj_template->add("MEMORY_COST", fvalue);
    }

    if ( user_obj_template->get("DISK_COST", fvalue) == true )
    {
        if ( fvalue < 0 )
        {
            goto error_disk_cost;
        }

        user_obj_template->erase("DISK_COST");
        obj_template->add("DISK_COST", fvalue);
    }

    // ------------------------------------------------------------------------
    // Check the OS attribute
    // ------------------------------------------------------------------------
    rc = parse_os(error_str);

    if ( rc != 0 )
    {
        goto error_os;
    }

    // ------------------------------------------------------------------------
    // Check the CPU Model attribute
    // ------------------------------------------------------------------------
    parse_cpu_model(user_obj_template.get());

    // ------------------------------------------------------------------------
    // Validate RAW attribute
    // ------------------------------------------------------------------------
    rc = Nebula::instance().get_vmm()->validate_raw(obj_template.get(), error_str);

    if (rc != 0)
    {
        goto error_raw;
    }

    // ------------------------------------------------------------------------
    // PCI Devices (Needs to be parsed before network)
    // ------------------------------------------------------------------------
    rc = parse_pci(error_str, user_obj_template.get());

    if ( rc != 0 )
    {
        goto error_pci;
    }

    // ------------------------------------------------------------------------
    // Parse the defaults to merge
    // ------------------------------------------------------------------------
    rc = parse_defaults(error_str, user_obj_template.get());

    if ( rc != 0 )
    {
        goto error_defaults;
    }

    // ------------------------------------------------------------------------
    // Parse the virtual router attributes
    // ------------------------------------------------------------------------
    rc = parse_vrouter(error_str, user_obj_template.get());

    if ( rc != 0 )
    {
        goto error_vrouter;
    }

    // ------------------------------------------------------------------------
    // Get network leases
    // ------------------------------------------------------------------------
    rc = get_network_leases(error_str);

    if ( rc != 0 )
    {
        goto error_leases_rollback;
    }

    // ------------------------------------------------------------------------
    // Get disk images
    // ------------------------------------------------------------------------
    rc = get_disk_images(error_str);

    if ( rc != 0 )
    {
        // The get_disk_images method has an internal rollback for
        // the acquired images, release_disk_images() would release all disks
        goto error_leases_rollback;
    }

    bool on_hold;

    if (user_obj_template->get("SUBMIT_ON_HOLD", on_hold) == true)
    {
        user_obj_template->erase("SUBMIT_ON_HOLD");

        obj_template->replace("SUBMIT_ON_HOLD", on_hold);
    }

    if ( has_cloning_disks())
    {
        state = VirtualMachine::CLONING;
    }

    // -------------------------------------------------------------------------
    // Set boot order
    // -------------------------------------------------------------------------
    rc = set_boot_order(obj_template.get(), error_str);

    if ( rc != 0 )
    {
        goto error_boot_order;
    }

    // ------------------------------------------------------------------------
    // Parse the backup attribute. It assumes volatile disks will take part of
    // the backups
    // ------------------------------------------------------------------------
    rc = _backups.parse(user_obj_template.get(), disks.backup_increment(true),
                        disks.backup_keep_last(true), false, error_str);

    if ( rc != 0 )
    {
        goto error_backup;
    }

    // -------------------------------------------------------------------------
    // Parse the context & requirements
    // -------------------------------------------------------------------------
    rc = parse_context(error_str, false); //Don't generate context for auto NICs

    if ( rc != 0 )
    {
        goto error_context;
    }

    rc = parse_requirements(error_str);

    if ( rc != 0 )
    {
        goto error_requirements;
    }

    rc = automatic_requirements(cluster_ids, error_str);

    if ( rc != 0 )
    {
        goto error_requirements;
    }

    if ( parse_graphics(error_str, user_obj_template.get()) != 0 )
    {
        goto error_graphics;
    }

    if ( parse_video(error_str, user_obj_template.get()) != 0 )
    {
        goto error_video;
    }

    // -------------------------------------------------------------------------
    // Get and set DEPLOY_ID for imported VMs
    // -------------------------------------------------------------------------
    user_obj_template->get("DEPLOY_ID", value);
    user_obj_template->erase("DEPLOY_ID");

    if (!value.empty())
    {
        const char * one_vms = "^one-[[:digit:]]+$";

        if (one_util::regex_match(one_vms, value.c_str()) == 0)
        {
            goto error_one_vms;
        }
        else
        {
            obj_template->add("IMPORTED", "YES");
            deploy_id = value;
        }
    }

    // ------------------------------------------------------------------------
    // Associate to VM Group
    // ------------------------------------------------------------------------
    if ( get_vmgroup(error_str) == -1 )
    {
        goto error_rollback;
    }

    // Encrypt all the secrets
    encrypt();

    // ------------------------------------------------------------------------
    // Insert the VM
    // ------------------------------------------------------------------------
    rc = insert_replace(db, false, error_str);

    if ( rc != 0 )
    {
        goto error_update;
    }

    //-------------------------------------------------------------------------
    //Check for missing unlock callbacks during creation
    //-------------------------------------------------------------------------
    if ( state == VirtualMachine::CLONING )
    {
        Nebula::instance().get_lcm()->trigger_disk_lock_success(oid);
    }

    return 0;

error_update:
error_boot_order:
error_context:
error_requirements:
error_graphics:
error_video:
error_backup:
error_rollback:
    release_disk_images(quotas, true);

error_leases_rollback:
    release_network_leases();
    goto error_common;

error_cpu:
    error_str = "CPU attribute must be a positive float or integer value.";
    goto error_common;

error_vcpu:
    error_str = "VCPU attribute must be a positive integer value.";
    goto error_common;

error_memory:
    error_str = "MEMORY attribute must be a positive integer value.";
    goto error_common;

error_cpu_cost:
    error_str = "CPU_COST attribute must be a positive float or integer value.";
    goto error_common;

error_memory_cost:
    error_str = "MEMORY_COST attribute must be a positive float or integer value.";
    goto error_common;

error_disk_cost:
    error_str = "DISK_COST attribute must be a positive float or integer value.";
    goto error_common;

error_one_vms:
    error_str = "Trying to import an OpenNebula VM: 'one-*'.";
    goto error_common;

error_raw:
error_os:
error_pci:
error_defaults:
error_vrouter:
error_name:
error_mem_mode:
error_common:
    NebulaLog::log("ONE", Log::ERROR, error_str);

    return -1;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

/**
 * @return -1 for incompatible datastore IDs, -2 for missing datastore IDs
 */
static int check_and_set_datastores_id(const set<int> &csystem_ds,
                                       set<int> &ds_ids)
{
    if ( csystem_ds.empty() )
    {
        return -2;
    }

    if ( ds_ids.empty() )
    {
        ds_ids = csystem_ds;

        return 0;
    }

    set<int> intersection = one_util::set_intersection(ds_ids, csystem_ds);

    if (intersection.empty())
    {
        return -1;
    }

    ds_ids = intersection;

    return 0;
}

/* ------------------------------------------------------------------------ */


/**
 * @return -1 for incompatible cluster IDs, -2 for missing cluster IDs
 */
static int check_and_set_cluster_id(
        const char *           id_name,
        const VectorAttribute* vatt,
        set<int>               &cluster_ids)
{
    set<int> vatt_cluster_ids;
    string   val;

    // If the attr does not exist, the vatt is using a manual path/resource.
    // This is different to a resource with 0 clusters
    if (vatt->vector_value(id_name, val) != 0)
    {
        return 0;
    }

    one_util::split_unique(val, ',', vatt_cluster_ids);

    if ( vatt_cluster_ids.empty() )
    {
        return -2;
    }

    if ( cluster_ids.empty() )
    {
        cluster_ids = vatt_cluster_ids;

        return 0;
    }

    set<int> intersection = one_util::set_intersection(cluster_ids,
                                                       vatt_cluster_ids);

    if (intersection.empty())
    {
        return -1;
    }

    cluster_ids = intersection;

    return 0;
}

/* ------------------------------------------------------------------------ */

void update_os_file(VectorAttribute *  os,
                    const string&      base_name)
{
    ClusterPool *   clpool = Nebula::instance().get_clpool();
    int             ds_id;
    set<int>        cluster_ids;

    string base_name_ds_id   = base_name + "_DS_DSID";
    string base_name_cluster = base_name + "_DS_CLUSTER_ID";

    if (os->vector_value(base_name_ds_id, ds_id) != 0)
    {
        return;
    }

    clpool->query_datastore_clusters(ds_id, cluster_ids);

    os->replace(base_name_cluster, one_util::join(cluster_ids, ','));
}

/* ------------------------------------------------------------------------ */

static void update_disk_cluster_id(VectorAttribute* disk)
{
    ClusterPool *   clpool = Nebula::instance().get_clpool();
    int             ds_id;
    set<int>        cluster_ids;

    if (disk->vector_value("DATASTORE_ID", ds_id) != 0)
    {
        return;
    }

    clpool->query_datastore_clusters(ds_id, cluster_ids);

    disk->replace("CLUSTER_ID", one_util::join(cluster_ids, ','));
}

/* ------------------------------------------------------------------------ */

static void update_nic_cluster_id(VectorAttribute* nic)
{
    ClusterPool *   clpool = Nebula::instance().get_clpool();
    int             vn_id;
    set<int>        cluster_ids;

    if (nic->vector_value("NETWORK_ID", vn_id) != 0)
    {
        return;
    }

    clpool->query_vnet_clusters(vn_id, cluster_ids);

    nic->replace("CLUSTER_ID", one_util::join(cluster_ids, ','));
}

/* ------------------------------------------------------------------------ */

/**
 * Returns the list of Cluster IDs where the VM can be deployed, based
 * on the Datastores and VirtualNetworks requested
 *
 * @param tmpl of the VirtualMachine
 * @param cluster_ids set of Cluster IDs
 * @param error_str Returns the error reason, if any
 * @return 0 on success
 */
static int get_cluster_requirements(Template *tmpl, set<int>& cluster_ids,
                                    string& error_str)
{
    ostringstream   oss;
    int             num_vatts;
    vector<VectorAttribute*> vatts;

    int incomp_id;
    int rc;

    // Get cluster id from the KERNEL and INITRD (FILE Datastores)
    VectorAttribute * osatt = tmpl->get("OS");

    if ( osatt != 0 )
    {
        update_os_file(osatt, "KERNEL");

        rc = check_and_set_cluster_id("KERNEL_DS_CLUSTER_ID", osatt, cluster_ids);

        if ( rc != 0 )
        {
            goto error_kernel;
        }

        update_os_file(osatt, "INITRD");

        rc = check_and_set_cluster_id("INITRD_DS_CLUSTER_ID", osatt, cluster_ids);

        if ( rc != 0 )
        {
            goto error_initrd;
        }
    }

    // Get cluster id from all DISK vector attributes (IMAGE Datastore)
    num_vatts = tmpl->get("DISK", vatts);

    for (int i=0; i<num_vatts; i++)
    {
        update_disk_cluster_id(vatts[i]);

        rc = check_and_set_cluster_id("CLUSTER_ID", vatts[i], cluster_ids);

        if ( rc != 0 )
        {
            incomp_id = i;
            goto error_disk;
        }
    }

    vatts.clear();

    // Get cluster id from all NIC vector attributes
    num_vatts = tmpl->get("NIC", vatts);

    for (int i=0; i<num_vatts; i++)
    {
        update_nic_cluster_id(vatts[i]);

        rc = check_and_set_cluster_id("CLUSTER_ID", vatts[i], cluster_ids);

        if ( rc != 0 )
        {
            incomp_id = i;
            goto error_nic;
        }
    }

    vatts.clear();

    // Get cluster id from all PCI attibutes, TYPE = NIC
    num_vatts = tmpl->get("PCI", vatts);

    for (int i=0; i<num_vatts; i++)
    {
        if ( vatts[i]->vector_value("TYPE") != "NIC" )
        {
            continue;
        }

        update_nic_cluster_id(vatts[i]);

        rc = check_and_set_cluster_id("CLUSTER_ID", vatts[i], cluster_ids);

        if ( rc != 0 )
        {
            incomp_id = i;
            goto error_pci;
        }
    }

    return 0;

error_disk:
    if (rc == -1)
    {
        oss << "Incompatible clusters in DISK. Datastore for DISK "<< incomp_id
            << " is not in the same cluster as the one used by other VM elements "
            << "(cluster " << one_util::join(cluster_ids, ',') << ")";
    }
    else
    {
        oss << "Missing clusters. Datastore for DISK "<< incomp_id
            << " is not in any cluster";
    }

    goto error_common;

error_kernel:
    if (rc == -1)
    {
        oss<<"Incompatible cluster in KERNEL datastore, it should be in cluster "
           << one_util::join(cluster_ids, ',') << ".";
    }
    else
    {
        oss << "Missing clusters. KERNEL datastore is not in any cluster.";
    }

    goto error_common;

error_initrd:
    if (rc == -1)
    {
        oss<<"Incompatible cluster in INITRD datastore, it should be in cluster "
           << one_util::join(cluster_ids, ',') << ".";
    }
    else
    {
        oss << "Missing clusters. INITRD datastore is not in any cluster.";
    }

    goto error_common;

error_nic:
    if (rc == -1)
    {
        oss << "Incompatible clusters in NIC. Network for NIC "<< incomp_id
            << " is not in the same cluster as the one used by other VM elements "
            << "(cluster " << one_util::join(cluster_ids, ',') << ")";
    }
    else
    {
        oss << "Missing clusters. Network for NIC "<< incomp_id
            << " is not in any cluster";
    }

    goto error_common;

error_pci:
    if (rc == -1)
    {
        oss << "Incompatible clusters in PCI (TYPE=NIC). Network for PCI "
            << incomp_id
            << " is not in the same cluster as the one used by other VM elements "
            << "(cluster " << one_util::join(cluster_ids, ',') << ")";
    }
    else
    {
        oss << "Missing clusters. Network for PCI "<< incomp_id
            << " is not in any cluster";
    }

    goto error_common;

error_common:
    error_str = oss.str();

    return -1;
}

/* ------------------------------------------------------------------------ */

/**
 * Returns the list of Datastore IDs where the VM can be deployed, based
 * on the images and his image datastores
 *
 * @param tmpl of the VirtualMachine
 * @param datastore_ids set of Cluster IDs
 * @param error_str Returns the error reason, if any
 * @return 0 on success
 */
static int get_datastore_requirements(Template *tmpl, set<int>& ds_ids,
                                      bool& shareable, string& error_str)
{
    ostringstream oss;

    vector<VectorAttribute*> vatts;
    set<int> csystem_ds;
    shareable = false;

    DatastorePool * ds_pool = Nebula::instance().get_dspool();

    int incomp_id;

    // Get cluster id from all DISK vector attributes (IMAGE Datastore)
    int num_vatts = tmpl->get("DISK", vatts);

    for (int i=0; i<num_vatts; i++, csystem_ds.clear())
    {
        bool is_shareable;
        vatts[i]->vector_value("SHAREABLE", is_shareable);
        shareable = shareable || is_shareable;

        int val;

        if (vatts[i]->vector_value("DATASTORE_ID", val) != 0)
        {
            continue;
        }

        if (auto ds = ds_pool->get_ro(val))
        {
            ds->get_compatible_system_ds(csystem_ds);

            int rc = check_and_set_datastores_id(csystem_ds, ds_ids);

            if ( rc == -1 )
            {
                incomp_id = i;
                goto error_disk;
            }
        }
    }

    return 0;

error_disk:
    oss << "Incompatible system datastore in DISK. Images Datastore for DISK "
        << incomp_id
        << " has not the same complatible system datastore"
        << "(system datastores " << one_util::join(ds_ids, ',') << ")";

    error_str = oss.str();

    return -1;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

int VirtualMachine::automatic_requirements(set<int>& cluster_ids,
                                           string& error_str)
{
    ostringstream   oss;
    bool            shareable = false;
    set<int>        datastore_ids;

    obj_template->erase("AUTOMATIC_REQUIREMENTS");
    obj_template->erase("AUTOMATIC_DS_REQUIREMENTS");
    obj_template->erase("AUTOMATIC_NIC_REQUIREMENTS");

    int rc = get_cluster_requirements(obj_template.get(), cluster_ids, error_str);

    if (rc != 0)
    {
        return -1;
    }

    rc = get_datastore_requirements(obj_template.get(), datastore_ids,
                                    shareable, error_str);

    if (rc == -1)
    {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Set automatic Host requirements
    // -------------------------------------------------------------------------
    if ( !cluster_ids.empty() )
    {
        auto i = cluster_ids.begin();

        oss << "(CLUSTER_ID = " << *i;

        for (++i; i != cluster_ids.end(); i++)
        {
            oss << " | CLUSTER_ID = " << *i;
        }

        oss << ") & ";
    }

    if ( is_pinned() )
    {
        oss << "(PIN_POLICY = PINNED)";
    }
    else
    {
        oss << "!(PIN_POLICY = PINNED)";
    }

    const VectorAttribute * cpu_model = obj_template->get("CPU_MODEL");

    if ( cpu_model != nullptr )
    {
        vector<string> features_v;
        const string&  features_s = cpu_model->vector_value("FEATURES");

        one_util::split(features_s, ',', features_v);

        for (const auto& feature: features_v)
        {
            oss << " & (KVM_CPU_FEATURES = \"*" << feature << "*\")";
        }
    }

    obj_template->add("AUTOMATIC_REQUIREMENTS", oss.str());

    oss.str("");

    // -------------------------------------------------------------------------
    // Set automatic System DS requirements
    // -------------------------------------------------------------------------

    if ( !cluster_ids.empty() || !datastore_ids.empty() )
    {
        if ( !cluster_ids.empty() )
        {
            auto i = cluster_ids.begin();

            oss << "(\"CLUSTERS/ID\" @> " << *i;

            for (++i; i != cluster_ids.end(); i++)
            {
                oss << " | \"CLUSTERS/ID\" @> " << *i;
            }

            oss << ")";

            obj_template->add("AUTOMATIC_NIC_REQUIREMENTS", oss.str());

            if ( !datastore_ids.empty() )
            {
                oss << " & ";
            }
        }

        if ( !datastore_ids.empty() )
        {
            auto i = datastore_ids.begin();

            oss << "(\"ID\" @> " << *i;

            for (++i; i != datastore_ids.end(); i++)
            {
                oss << " | \"ID\" @> " << *i;
            }

            oss << ")";

        }

        std::string tm_mad_system;
        if ( obj_template->get("TM_MAD_SYSTEM", tm_mad_system) )
        {
            oss << " & (TM_MAD = \"" << one_util::trim(tm_mad_system) << "\")";
        }
    }

    if (shareable)
    {
        if (!oss.str().empty())
        {
            oss << " & ";
        }

        oss << "(SHARED = \"YES\")";
    }

    if (!oss.str().empty())
    {
        obj_template->add("AUTOMATIC_DS_REQUIREMENTS", oss.str());
    }

    return 0;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

int VirtualMachine::insert_replace(SqlDB *db, bool replace, string& error_str)
{
    ostringstream   oss;
    int             rc;

    string xml_body, short_xml_body, text;

    char * sql_name      = nullptr;
    char * sql_xml       = nullptr;
    char * sql_short_xml = nullptr;
    char * sql_text      = nullptr;

    sql_name =  db->escape_str(name);

    if ( sql_name == 0 )
    {
        goto error_generic;
    }

    sql_xml = db->escape_str(to_xml(xml_body));

    if ( sql_xml == 0 )
    {
        goto error_body;
    }

    if ( validate_xml(sql_xml) != 0 )
    {
        goto error_xml;
    }

    sql_short_xml = db->escape_str(to_xml_short(short_xml_body));

    if ( sql_short_xml == 0 )
    {
        goto error_body_short;
    }

    if ( validate_xml(sql_short_xml) != 0 )
    {
        goto error_xml_short;
    }

    sql_text = db->escape_str(to_json(text));

    if ( sql_text == 0 )
    {
        goto error_text;
    }

    if (replace)
    {
        oss << "UPDATE " << one_db::vm_table << " SET "
            << "name = '"         <<  sql_name      << "', "
            << "body = '"         <<  sql_xml       << "', "
            << "uid = "           <<  uid           << ", "
            << "gid = "           <<  gid           << ", "
            << "state = "         <<  state         << ", "
            << "lcm_state = "     <<  lcm_state     << ", "
            << "resched = "       <<  resched       << ", "
            << "owner_u = "       <<  owner_u       << ", "
            << "group_u = "       <<  group_u       << ", "
            << "other_u = "       <<  other_u       << ", "
            << "short_body = '"   <<  sql_short_xml << "', "
            << "body_json = '"    <<  sql_text      << "' "
            << "WHERE oid = "     <<  oid;
    }
    else
    {
        oss << "INSERT INTO " << one_db::vm_table
            << " ("<< one_db::vm_db_names << ") VALUES ("
            <<        oid           << ","
            << "'" << sql_name      << "',"
            << "'" << sql_xml       << "',"
            <<        uid           << ","
            <<        gid           << ","
            <<        state         << ","
            <<        lcm_state     << ","
            <<        resched        << ","
            <<        owner_u       << ","
            <<        group_u       << ","
            <<        other_u       << ","
            << "'" << sql_short_xml << "',"
            << "'" << sql_text      << "'"
            << ")";
    }

    db->free_str(sql_name);
    db->free_str(sql_xml);
    db->free_str(sql_short_xml);
    db->free_str(sql_text);

    rc = db->exec_wr(oss);

    return rc;

error_text:
    db->free_str(sql_text);
error_xml_short:
    db->free_str(sql_short_xml);
error_xml:
    db->free_str(sql_name);
    db->free_str(sql_xml);

    error_str = "Error transforming the VM to XML.";

    goto error_common;

error_body_short:
    db->free_str(sql_xml);
error_body:
    db->free_str(sql_name);
    goto error_generic;

error_generic:
    error_str = "Error inserting VM in DB.";
error_common:
    return -1;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::update_search(SqlDB * db)
{
    std::ostringstream oss;
    std::string text;

    char * sql_text = db->escape_str(to_json(text));

    if ( sql_text == 0 )
    {
        NebulaLog::log("ONE", Log::ERROR, "Error updating VM search token");
        return -1;
    }

    oss << "UPDATE " << one_db::vm_table << " SET "
        << "body_json = '" << sql_text << "' "
        << "WHERE oid = " << oid;

    db->free_str(sql_text);

    return db->exec_wr(oss);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::add_history(
        int   hid,
        int   cid,
        const string& hostname,
        const string& vmm_mad,
        const string& tm_mad,
        int           ds_id)
{
    int           seq;
    string        vm_xml;

    if (!history)
    {
        seq = 0;
    }
    else
    {
        seq = history->seq + 1;

        previous_history = move(history);
    }

    to_xml_extended(vm_xml, 0, false);

    history = make_unique<History>(oid, seq, hid, hostname, cid, vmm_mad, tm_mad,
                                   ds_id, -2, -1, vm_xml);
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::cp_history()
{
    string    vm_xml;

    if (history == 0)
    {
        return;
    }

    to_xml_extended(vm_xml, 0, false);

    auto htmp = make_unique<History>(oid,
                       history->seq + 1,
                       history->hid,
                       history->hostname,
                       history->cid,
                       history->vmm_mad_name,
                       history->tm_mad_name,
                       history->ds_id,
                       history->plan_id,
                       history->action_id,
                       vm_xml);

    previous_history = move(history);
    history          = move(htmp);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::cp_previous_history()
{
    string    vm_xml;

    if ( !previous_history || !history )
    {
        return;
    }

    to_xml_extended(vm_xml, 0, false);

    auto htmp = make_unique<History>(oid,
                       history->seq + 1,
                       previous_history->hid,
                       previous_history->hostname,
                       previous_history->cid,
                       previous_history->vmm_mad_name,
                       previous_history->tm_mad_name,
                       previous_history->ds_id,
                       previous_history->plan_id,
                       previous_history->action_id,
                       vm_xml);

    previous_history = move(history);
    history          = move(htmp);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::get_capacity(HostShareCapacity& sr) const
{
    sr.set(oid, *obj_template);
}

void VirtualMachine::get_previous_capacity(HostShareCapacity& sr, Template &tmpl) const
{
    if (!previous_history)
    {
        return;
    }

    const string& vm_info = previous_history->vm_info;

    ObjectXML xml(vm_info);

    vector<xmlNodePtr> content;
    xml.get_nodes("/VM/TEMPLATE", content);

    if (content.empty())
    {
        NebulaLog::error("ONE", "Unable to read capacity from previous history");

        return;
    }

    auto rc = tmpl.from_xml_node(content[0]);

    if (rc != 0)
    {
        NebulaLog::error("ONE", "Unable to parse capacity from previous history");

        return;
    }

    sr.set(oid, tmpl);

    ObjectXML::free_nodes(content);

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::resize(float cpu, long int memory, unsigned int vcpu,
                           string& error)
{
    unsigned int s, c, t;

    s = c = t = 0;

    if (cpu > 0)
    {
        replace_template_attribute("CPU", cpu);
    }

    if (memory > 0)
    {
        replace_template_attribute("MEMORY", memory);
    }

    if (vcpu > 0)
    {
        replace_template_attribute("VCPU", vcpu);
    }
    else
    {
        get_template_attribute("VCPU", vcpu);
    }

    /* ---------------------------------------------------------------------- */
    /* Update the NUMA topology with new size:                                */
    /*   1. Increase number of cores for new VCPU (keep threads and sockets)  */
    /*   2. Clear current nodes and build new ones with new MEMORY/VCPU       */
    /* ---------------------------------------------------------------------- */
    VectorAttribute * vtopol = get_template_attribute("TOPOLOGY");

    if ( vtopol == 0 )
    {
        return 0;
    }

    vtopol->vector_value("SOCKETS", s);
    vtopol->vector_value("CORES", c);
    vtopol->vector_value("THREADS", t);

    if ( s != 0 && c != 0 && t != 0 && (s * c * t) != vcpu)
    {
        if ( vcpu%(t * s) != 0 )
        {
            error = "new VCPU is not multiple of the total number of threads";
            return -1;
        }

        c = vcpu/(t * s);

        vtopol->replace("CORES", c);
    }

    remove_template_attribute("NUMA_NODE");

    return parse_topology(obj_template.get(), error);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::store_resize(float cpu, long int memory, unsigned int vcpu)
{
    auto vattr = new VectorAttribute("RESIZE");
    vattr->replace("CPU", cpu);
    vattr->replace("VCPU", vcpu);
    vattr->replace("MEMORY", memory);

    obj_template->erase("RESIZE");
    obj_template->set(vattr);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::reset_resize()
{
    obj_template->erase("RESIZE");
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

static int parse_memory_mode(string& mem_mode, string& error)
{
    one_util::toupper(mem_mode);

    if (mem_mode != "BALLOONING" && mem_mode != "HOTPLUG")
    {
        error = "Unknown MEMORY_RESIZE_MODE: " + mem_mode;

        return -1;
    }

    return 0;
}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::nic_update(int vnid)
{
    auto vnpool = Nebula::instance().get_vnpool();
    auto vn     = vnpool->get_ro(vnid);

    unique_ptr<VectorAttribute> updating;

    if (!vn)
    {
        return -1;
    }

    for (auto nic : nics)
    {
        if (nic->is_alias())
        {
            continue;
        }

        int nic_vnid;

        if (nic->vector_value("NETWORK_ID", nic_vnid) == 0 && vnid == nic_vnid)
        {
            int rc = vn->nic_update(nic, updating);

            if (rc < 0)
            {
                return -1;
            }
        }
    }

    auto vnet_update = obj_template->get("VNET_UPDATE");

    if (vnet_update)
    {
        // Merge with previous VNET_UPDATE
        if (updating)
        {
            vnet_update->merge(updating.get(), true);
        }
    }
    else if (updating)
    {
        obj_template->set(updating.release());
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::nic_update(int nic_id, VirtualMachineNic *new_nic, bool live)
{
    static const set<string> UPDATE_ATTRIBUTES = { "INBOUND_AVG_BW",
                                                   "INBOUND_PEAK_BW", "INBOUND_PEAK_KB", "OUTBOUND_AVG_BW",
                                                   "OUTBOUND_PEAK_BW", "OUTBOUND_PEAK_KB"
                                                 };

    VirtualMachineNic *nic = get_nic(nic_id);

    if (!nic)
    {
        return -1;
    }

    int num_updated = 0;

    VectorAttribute* update_attr = obj_template->get("VNET_UPDATE");

    if (live && !update_attr)
    {
        update_attr = new VectorAttribute("VNET_UPDATE");

        obj_template->set(update_attr);
    }

    // Detect new and modified attributes
    for (const auto& attr : new_nic->vector_attribute()->value())
    {
        string old_value;

        if ( UPDATE_ATTRIBUTES.count(attr.first) == 1 &&
             (nic->vector_value(attr.first, old_value) != 0
              || attr.second != old_value) )
        {
            num_updated++;

            nic->replace(attr.first, attr.second);

            if (live)
            {
                update_attr->replace(attr.first, old_value);
            }
        }
    }

    // Detect removed attributes
    vector<string> to_remove;

    for (const auto& attr : nic->vector_attribute()->value())
    {
        string new_value;

        if ( UPDATE_ATTRIBUTES.count(attr.first) == 1  &&
             new_nic->vector_value(attr.first, new_value) != 0)
        {
            num_updated++;

            to_remove.push_back(attr.first);

            if (live)
            {
                update_attr->replace(attr.first, attr.second);
            }
        }
    }

    for (const auto& attr : to_remove)
    {
        nic->remove(attr);
    }

    return num_updated;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::remove_security_group(int sgid)
{
    int num_sgs;
    int ssgid;

    vector<VectorAttribute  *> sgs;

    num_sgs = obj_template->get("SECURITY_GROUP_RULE", sgs);

    for (int i=0; i<num_sgs; i++)
    {
        sgs[i]->vector_value("SECURITY_GROUP_ID", ssgid);

        if ( ssgid == sgid )
        {
            obj_template->remove(sgs[i]);
            delete sgs[i];
        }
    }

    SecurityGroupPool* sgpool = Nebula::instance().get_secgrouppool();
    sgpool->release_security_group(oid, sgid);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::get_vrouter_id() const
{
    int vrid;

    if (!obj_template->get("VROUTER_ID", vrid))
    {
        vrid = -1;
    }

    return vrid;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

bool VirtualMachine::is_vrouter() const
{
    return get_vrouter_id() != -1;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::set_auth_request(int uid,
                                      AuthRequest& ar,
                                      VirtualMachineTemplate *tmpl,
                                      bool check_lock)
{
    VirtualMachineDisks tdisks(tmpl, false);

    for (auto disk : tdisks)
    {
        disk->authorize(uid, &ar, check_lock);
    }

    VirtualMachineNics tnics(tmpl);

    for (auto nic : tnics)
    {
        nic->authorize(uid, &ar, check_lock);
    }

    const VectorAttribute * vmgroup = tmpl->get("VMGROUP");

    if ( vmgroup != 0 )
    {
        VMGroupPool * vmgrouppool = Nebula::instance().get_vmgrouppool();

        vmgrouppool->authorize(vmgroup, uid, &ar);
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

string& VirtualMachine::to_xml_extended(string& xml, int n_history, bool sa) const
{
    string template_xml;
    string user_template_xml;
    string perm_xml;
    string snap_xml;
    string bck_xml;
    string lock_str;
    string sas_xml;
    string tmp_xml;

    ostringstream oss;

    if ( sa )
    {
        ScheduledActionPool * sa_pool = Nebula::instance().get_sapool();

        sa_pool->dump(_sched_actions.get_collection(), sas_xml);
    }

    oss << "<VM>"
        << "<ID>"        << oid       << "</ID>"
        << "<UID>"       << uid       << "</UID>"
        << "<GID>"       << gid       << "</GID>"
        << "<UNAME>"     << uname     << "</UNAME>"
        << "<GNAME>"     << gname     << "</GNAME>"
        << "<NAME>"      << name      << "</NAME>"
        << perms_to_xml(perm_xml)
        << "<LAST_POLL>" << monitoring.timestamp() << "</LAST_POLL>"
        << "<STATE>"     << state     << "</STATE>"
        << "<LCM_STATE>" << lcm_state << "</LCM_STATE>"
        << "<PREV_STATE>"     << prev_state     << "</PREV_STATE>"
        << "<PREV_LCM_STATE>" << prev_lcm_state << "</PREV_LCM_STATE>"
        << "<RESCHED>"   << resched   << "</RESCHED>"
        << "<STIME>"     << stime     << "</STIME>"
        << "<ETIME>"     << etime     << "</ETIME>"
        << "<DEPLOY_ID>" << deploy_id << "</DEPLOY_ID>"
        << lock_db_to_xml(lock_str)
        << monitoring.to_xml()
        << _sched_actions.to_xml(tmp_xml)
        << obj_template->to_xml(template_xml, sas_xml)
        << user_obj_template->to_xml(user_template_xml);

    if ( hasHistory() && n_history > 0 )
    {
        string history_xml;

        if ( n_history == 2 )
        {
            if (history && history->seq > 1)
            {
                // Dump the full history record
                auto vmpool = Nebula::instance().get_vmpool();
                vmpool->dump_history(history_xml, oid);

                // Remove the VM from the history to reduce size and comply with xml-schema
                ObjectXML obj(history_xml);
                obj.remove_nodes("/HISTORY_RECORDS/HISTORY/VM");

                oss << obj;
            }
            else
            {
                oss << "<HISTORY_RECORDS>";
                if (previous_history) oss << previous_history->to_xml(history_xml);
                if (history) oss << history->to_xml(history_xml);
                oss << "</HISTORY_RECORDS>";
            }
        }
        else
        {
            oss << "<HISTORY_RECORDS>";
            oss << history->to_xml(history_xml);
            oss << "</HISTORY_RECORDS>";
        }
    }
    else
    {
        oss << "<HISTORY_RECORDS/>";
    }

    for (auto disk = const_cast<VirtualMachineDisks *>(&disks)->begin() ;
         disk != const_cast<VirtualMachineDisks *>(&disks)->end() ; ++disk)
    {
        const Snapshots * snapshots = (*disk)->get_snapshots();

        if ( snapshots != 0 )
        {
            oss << snapshots->to_xml(snap_xml);
        }
    }

    oss << _backups.to_xml(bck_xml);

    oss << "</VM>";

    xml = oss.str();

    return xml;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

string& VirtualMachine::to_json(string& json) const
{
    string template_json;
    string user_template_json;
    string history_json;

    ostringstream oss;

    oss << "{\"VM\":{"
        << "\"ID\": \""<< oid << "\","
        << "\"UID\": \""<< uid << "\","
        << "\"GID\": \""<< gid << "\","
        << "\"UNAME\": \""<< uname << "\","
        << "\"GNAME\": \""<< gname << "\","
        << "\"NAME\": \""<< name << "\","
        << "\"LAST_POLL\": \""<< monitoring.timestamp() << "\","
        << "\"STATE\": \""<< state << "\","
        << "\"LCM_STATE\": \""<< lcm_state << "\","
        << "\"PREV_STATE\": \""<< prev_state << "\","
        << "\"PREV_LCM_STATE\": \""<< prev_lcm_state << "\","
        << "\"RESCHED\": \""<< resched << "\","
        << "\"STIME\": \""<< stime << "\","
        << "\"ETIME\": \""<< etime << "\","
        << "\"DEPLOY_ID\": \""<< deploy_id << "\","
        << obj_template->to_json(template_json) << ","
        << user_obj_template->to_json(user_template_json);

    if ( hasHistory() )
    {
        oss << ",\"HISTORY_RECORDS\": { \"HISTORY\": [";

        oss << history->to_json(history_json);

        oss << "] }";
    }

    oss << "}}";


    json = oss.str();

    return json;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

string& VirtualMachine::to_xml_short(string& xml)
{
    string disks_xml, user_template_xml, history_xml, nics_xml;
    string cpu_tmpl, mem_tmpl, vcpu_tmpl;

    ostringstream   oss;

    obj_template->get("CPU", cpu_tmpl);
    obj_template->get("MEMORY", mem_tmpl);
    obj_template->get("VCPU", vcpu_tmpl);

    oss << "<VM>"
        << "<ID>"        << oid       << "</ID>"
        << "<UID>"       << uid       << "</UID>"
        << "<GID>"       << gid       << "</GID>"
        << "<UNAME>"     << uname     << "</UNAME>"
        << "<GNAME>"     << gname     << "</GNAME>"
        << "<NAME>"      << name      << "</NAME>"
        << "<LAST_POLL>" << monitoring.timestamp() << "</LAST_POLL>"
        << "<STATE>"     << state     << "</STATE>"
        << "<LCM_STATE>" << lcm_state << "</LCM_STATE>"
        << "<RESCHED>"   << resched   << "</RESCHED>"
        << "<STIME>"     << stime     << "</STIME>"
        << "<ETIME>"     << etime     << "</ETIME>"
        << "<DEPLOY_ID>" << deploy_id << "</DEPLOY_ID>";

    if (locked != PoolObjectSQL::ST_NONE)
    {
        oss << "<LOCK><LOCKED>" << static_cast<int>(locked) << "</LOCKED></LOCK>";
    }

    oss << "<TEMPLATE>"
        << "<CPU>"       << cpu_tmpl  << "</CPU>"
        << "<MEMORY>"    << mem_tmpl  << "</MEMORY>"
        << "<VCPU>"      << vcpu_tmpl << "</VCPU>"
        << disks.to_xml_short(disks_xml)
        << nics.to_xml_short(nics_xml);

    VectorAttribute * graph = obj_template->get("GRAPHICS");

    if ( graph != 0 )
    {
        graph->to_xml(oss);
    }

    oss << "</TEMPLATE>"
        << monitoring.to_xml_short()
        << user_obj_template->to_xml_short(user_template_xml);

    if ( hasHistory() )
    {
        oss << "<HISTORY_RECORDS>";
        oss << history->to_xml_short(history_xml);
        oss << "</HISTORY_RECORDS>";
    }
    else
    {
        oss << "<HISTORY_RECORDS/>";
    }

    oss << "</VM>";

    xml = oss.str();

    return xml;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::from_xml(const string &xml_str)
{
    vector<xmlNodePtr> content;

    int istate;
    int ilcmstate;
    int rc = 0;

    // Initialize the internal XML object
    rc = update_from_str(xml_str);

    if ( rc != 0 )
    {
        NebulaLog::error("ONE", "The VM XML body is corrupted");

        return -1;
    }

    // Get class base attributes
    rc += xpath(oid,       "/VM/ID",    -1);

    rc += xpath(uid,       "/VM/UID",   -1);
    rc += xpath(gid,       "/VM/GID",   -1);

    rc += xpath(uname,     "/VM/UNAME", "not_found");
    rc += xpath(gname,     "/VM/GNAME", "not_found");
    rc += xpath(name,      "/VM/NAME",  "not_found");

    rc += xpath(resched, "/VM/RESCHED", 0);

    rc += xpath<time_t>(stime, "/VM/STIME", 0);
    rc += xpath<time_t>(etime, "/VM/ETIME", 0);
    rc += xpath(deploy_id, "/VM/DEPLOY_ID", "");

    // Permissions
    rc += perms_from_xml();

    //VM states
    rc += xpath(istate,    "/VM/STATE",     0);
    rc += xpath(ilcmstate, "/VM/LCM_STATE", 0);

    state     = static_cast<VmState>(istate);
    lcm_state = static_cast<LcmState>(ilcmstate);

    xpath(istate,    "/VM/PREV_STATE",     istate);
    xpath(ilcmstate, "/VM/PREV_LCM_STATE", ilcmstate);

    prev_state     = static_cast<VmState>(istate);
    prev_lcm_state = static_cast<LcmState>(ilcmstate);

    rc += lock_db_from_xml();

    // -------------------------------------------------------------------------
    // Virtual Machine template and attributes
    // -------------------------------------------------------------------------
    ObjectXML::get_nodes("/VM/TEMPLATE", content);

    if (content.empty())
    {
        NebulaLog::error("ONE", "The VM " + to_string(oid) + " doesn't have TEMPLATE");

        return -1;
    }
    rc += obj_template->from_xml_node(content[0]);

    // Bug #6996: Remove Scheduled Action from the template
    obj_template->erase("SCHED_ACTION");

    vector<VectorAttribute *> vdisks, vnics, alias, pcis;

    obj_template->get("DISK", vdisks);

    disks.init(vdisks, true);

    obj_template->get("NIC", vnics);

    obj_template->get("NIC_ALIAS", alias);

    obj_template->get("PCI", pcis);

    for (auto vattr : pcis)
    {
        if ( vattr->vector_value("TYPE") == "NIC" )
        {
            vnics.push_back(vattr);
        }
    }

    for (auto vattr : alias)
    {
        vnics.push_back(vattr);
    }

    nics.init(vnics, true);

    ObjectXML::free_nodes(content);

    // -------------------------------------------------------------------------
    // Virtual Machine user template
    // -------------------------------------------------------------------------
    ObjectXML::get_nodes("/VM/USER_TEMPLATE", content);

    if (content.empty())
    {
        NebulaLog::error("ONE", "The VM " + to_string(oid) + " doesn't have USER_TEMPLATE");

        return -1;
    }

    rc += user_obj_template->from_xml_node(content[0]);

    ObjectXML::free_nodes(content);

    // -------------------------------------------------------------------------
    // Last history entry
    // -------------------------------------------------------------------------
    int last_seq;

    if ( xpath(last_seq, "/VM/HISTORY_RECORDS/HISTORY/SEQ", -1) == 0 &&
         last_seq != -1 )
    {
        history = make_unique<History>(oid, last_seq);

        // Initialize previous history
        --last_seq;
        if (last_seq >= 0)
        {
            previous_history = make_unique<History>(oid, last_seq);
        }
    }

    // -------------------------------------------------------------------------
    // Virtual Machine Snapshots
    // -------------------------------------------------------------------------
    ObjectXML::get_nodes("/VM/SNAPSHOTS", content);

    for (auto node : content)
    {
        Snapshots * snap = new Snapshots(-1, Snapshots::DENY);

        rc += snap->from_xml_node(node);

        if ( rc != 0)
        {
            delete snap;
            break;
        }

        disks.set_snapshots(snap->disk_id(), snap);
    }

    if (!content.empty())
    {
        ObjectXML::free_nodes(content);
    }

    rc += _backups.from_xml(this);

    _sched_actions.from_xml(this, "/VM/");

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    if (rc != 0)
    {
        NebulaLog::error("ONE", "Unable to recreate the VM " + to_string(oid)
                         + " from the xml body");

        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::load_monitoring()
{
    // Get last monitoring record
    auto vmpool = Nebula::instance().get_vmpool();
    monitoring = vmpool->get_monitoring(oid);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::replace_template(
        const string&   tmpl_str,
        bool            keep_restricted,
        string&         error)
{
    string ra;

    auto new_tmpl =
            make_unique<VirtualMachineTemplate>(false, '=', "USER_TEMPLATE");

    if ( new_tmpl->parse_str_or_xml(tmpl_str, error) != 0 )
    {
        return -1;
    }

    /* ---------------------------------------------------------------------- */
    /* Replace new_tmpl to the current user_template                          */
    /* ---------------------------------------------------------------------- */

    if (user_obj_template)
    {
        if (keep_restricted &&
            new_tmpl->check_restricted(ra, user_obj_template.get(), false))
        {
            error = "Tried to change restricted attribute: " + ra;

            return -1;
        }
    }
    else if (keep_restricted && new_tmpl->check_restricted(ra))
    {
        error = "Tried to set restricted attribute: " + ra;

        return -1;
    }

    auto old_user_tmpl = move(user_obj_template);
    user_obj_template  = move(new_tmpl);

    if (post_update_template(error, old_user_tmpl.get()) == -1)
    {
        user_obj_template = move(old_user_tmpl);

        return -1;
    }

    encrypt();

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::append_template(
        const string&   tmpl_str,
        bool            keep_restricted,
        string&         error)
{
    auto new_tmpl =
            make_unique<VirtualMachineTemplate>(false, '=', "USER_TEMPLATE");
    string rname;

    if ( new_tmpl->parse_str_or_xml(tmpl_str, error) != 0 )
    {
        return -1;
    }

    /* ---------------------------------------------------------------------- */
    /* Append new_tmpl to the current user_template                           */
    /* ---------------------------------------------------------------------- */
    // Create a copy of actual user template
    auto old_user_tmpl = make_unique<VirtualMachineTemplate>(*user_obj_template);

    if (keep_restricted &&
        new_tmpl->check_restricted(rname, user_obj_template.get(), true))
    {
        error ="User Template includes a restricted attribute " + rname;

        return -1;
    }

    user_obj_template->merge(new_tmpl.get());

    if (post_update_template(error, old_user_tmpl.get()) == -1)
    {
        user_obj_template = move(old_user_tmpl);

        return -1;
    }

    encrypt();

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::set_template_error_message(const string& message)
{
    set_template_error_message("ERROR", message);
}

/* -------------------------------------------------------------------------- */

void VirtualMachine::set_template_error_message(const string& name,
                                                const string& message)
{
    user_obj_template->erase(name);

    if (message.empty())
    {
        return;
    }

    ostringstream     error_value;

    error_value << one_util::log_time() << ": " << message.substr(0, MAX_ERROR_MSG_LENGTH);

    if (message.length() >= MAX_ERROR_MSG_LENGTH)
    {
        error_value << "... see more details in VM log";
    }

    auto attr = new SingleAttribute(name, error_value.str());

    user_obj_template->set(attr);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::clear_template_error_message()
{
    user_obj_template->erase("ERROR");
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

/**
 * Replaces the values of a vector value, preserving the existing ones
 */
static void replace_vector_values(Template *old_tmpl, Template *new_tmpl,
                                  const char * name, bool append)
{
    string value;

    VectorAttribute * new_attr = new_tmpl->get(name);
    VectorAttribute * old_attr = old_tmpl->get(name);

    if ( new_attr == 0 )
    {
        if ( !append )
        {
            old_tmpl->erase(name);
        }
    }
    else if ( old_attr == 0 )
    {
        old_tmpl->set(new_attr->clone());
    }
    else
    {
        std::vector<std::string> vnames = VirtualMachineTemplate::UPDATECONF_ATTRS[name];

        for (const auto& vname : vnames)
        {
            if ( new_attr->vector_value(vname, value) == -1 )
            {
                if ( !append )
                {
                    old_attr->remove(vname);
                }
            }
            else
            {
                old_attr->replace(vname, value);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::updateconf(VirtualMachineTemplate* tmpl, string &err,
                               bool append)
{
    bool context_changed = false;

    switch (state)
    {
        case PENDING:
        case HOLD:
        case POWEROFF:
        case UNDEPLOYED:
        case CLONING:
        case CLONING_FAILURE:
            break;

        case ACTIVE:
            switch (lcm_state)
            {
                case LCM_INIT:
                case PROLOG:
                case RUNNING:
                case EPILOG:
                case SHUTDOWN:
                case CLEANUP_RESUBMIT:
                case SHUTDOWN_POWEROFF:
                case CLEANUP_DELETE:
                case HOTPLUG_SAVEAS_POWEROFF:
                case SHUTDOWN_UNDEPLOY:
                case EPILOG_UNDEPLOY:
                case PROLOG_UNDEPLOY:
                case HOTPLUG_PROLOG_POWEROFF:
                case HOTPLUG_EPILOG_POWEROFF:
                case BOOT_FAILURE:
                case PROLOG_FAILURE:
                case EPILOG_FAILURE:
                case EPILOG_UNDEPLOY_FAILURE:
                case PROLOG_MIGRATE_POWEROFF:
                case PROLOG_MIGRATE_POWEROFF_FAILURE:
                case BOOT_UNDEPLOY_FAILURE:
                case PROLOG_UNDEPLOY_FAILURE:
                case DISK_SNAPSHOT_POWEROFF:
                case DISK_SNAPSHOT_REVERT_POWEROFF:
                case DISK_SNAPSHOT_DELETE_POWEROFF:
                    break;

                default:
                    err = "configuration cannot be updated in state " + state_str();
                    return -1;
            };
            break;

        case INIT:
        case DONE:
        case SUSPENDED:
        case STOPPED:

            err = "configuration cannot be updated in state " + state_str();
            return -1;
    }

    // -------------------------------------------------------------------------
    // Validates RAW data section
    // -------------------------------------------------------------------------
    if (Nebula::instance().get_vmm()->validate_raw(tmpl, err) != 0)
    {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Update OS, FEATURES, INPUT, GRAPHICS, VIDEO, RAW, CPU_MODEL
    // -------------------------------------------------------------------------
    replace_vector_values(obj_template.get(), tmpl, "OS", append);

    if ( set_boot_order(obj_template.get(), err) != 0 )
    {
        return -1;
    }

    replace_vector_values(obj_template.get(), tmpl, "FEATURES", append);

    replace_vector_values(obj_template.get(), tmpl, "INPUT", append);

    replace_vector_values(obj_template.get(), tmpl, "GRAPHICS", append);

    replace_vector_values(obj_template.get(), tmpl, "VIDEO", append);

    replace_vector_values(obj_template.get(), tmpl, "RAW", append);

    replace_vector_values(obj_template.get(), tmpl, "CPU_MODEL", append);

    // -------------------------------------------------------------------------
    // Update CONTEXT: any value
    // -------------------------------------------------------------------------
    VectorAttribute * context_bck = obj_template->get("CONTEXT");
    VectorAttribute * context_new = tmpl->get("CONTEXT");

    bool allow_eth_updates = false;
    Nebula::instance().get_configuration_attribute("CONTEXT_ALLOW_ETH_UPDATES", allow_eth_updates);

    if ( context_bck == 0 && context_new != 0 )
    {
        err = "Virtual machine does not have context, cannot add a new one.";

        return -1;
    }
    else if ( context_bck != 0 && context_new != 0 )
    {
        context_new = context_new->clone();

        // Remove equal values from the new context
        map<string, string> equal_values;
        map<string, string> eth_updates;

        auto in = context_new->value().cbegin();
        auto ib = context_bck->value().cbegin();

        for ( ; in != context_new->value().cend() &&
              ib != context_bck->value().cend() ; )
        {
            if (in->first < ib->first)
            {
                if (std::regex_match(in->first, regex("ETH\\d+_\\w+")))
                {
                    if (allow_eth_updates)
                    {
                        eth_updates.insert(make_pair(in->first, in->second));
                    }
                    else
                    {
                        // Do not allow add new attribute with name ETHx_y
                        err = "Unable to add " + in->first +
                              ", update NIC to update network context";

                        return -1;
                    }
                }

                context_changed = true;
                ++in;
            }
            else if (ib->first < in->first)
            {
                context_changed = context_changed || !append;
                ++ib;
            }
            else
            {
                if (in->second == ib->second)
                {
                    equal_values.insert(make_pair(in->first, in->second));
                }
                else
                {
                    if (std::regex_match(in->first, regex("ETH\\d+_\\w+")))
                    {
                        if (allow_eth_updates)
                        {
                            eth_updates.insert(make_pair(in->first, in->second));
                        }
                        else
                        {
                            // Do not allow update attribute with name ETHx_y
                            err = "Unable to update " + in->first +
                                  ", update NIC to update network context";

                            return -1;
                        }
                    }

                    context_changed = true;
                }

                ++in;
                ++ib;
            }
        }

        if ( in != context_new->value().cend() )
        {
            context_changed = true;
        }

        for (const auto& attr : equal_values)
        {
            if (!append && attr.first == "NETWORK")
            {
                // Regenerate NETWORK context in replace mode
                continue;
            }

            context_new->remove(attr.first);
        }

        context_new->replace("TARGET",  context_bck->vector_value("TARGET"));
        context_new->replace("DISK_ID", context_bck->vector_value("DISK_ID"));

        obj_template->remove(context_bck);
        obj_template->set(context_new);

        if ( parse_context(err, true) != 0 )
        {
            obj_template->erase("CONTEXT");
            obj_template->set(context_bck);

            return -1;
        }

        context_new = obj_template->get("CONTEXT");

        // Manual overrides of ETH* attributes
        for (const auto& eth_update : eth_updates)
        {
            context_new->replace(eth_update.first, eth_update.second);
        }

        if (append)
        {
            obj_template->remove(context_new);
            obj_template->set(context_bck);

            context_bck->merge(context_new, true);

            delete context_new;
        }
        else
        {
            for (const auto& attr : equal_values)
            {
                context_new->replace(attr.first, attr.second);
            }

            delete context_bck;
        }
    }

    // -------------------------------------------------------------------------
    // Parse graphics & video attribute
    // -------------------------------------------------------------------------
    if ( parse_graphics(err, obj_template.get()) != 0 )
    {
        NebulaLog::log("ONE", Log::ERROR, err);
        return -1;
    }

    if ( parse_video(err, obj_template.get()) != 0 )
    {
        NebulaLog::log("ONE", Log::ERROR, err);
        return -1;
    }

    // -------------------------------------------------------------------------
    // Parse backup configuration (if not doing a backup). Uses current value of
    // BACKUP_VOLATILE attribute.
    // -------------------------------------------------------------------------
    VectorAttribute * backup_conf = tmpl->get("BACKUP_CONFIG");

    if ( backup_conf != nullptr && lcm_state != BACKUP && lcm_state != BACKUP_POWEROFF)
    {
        bool do_volatile = _backups.do_volatile();
        bool increment = disks.backup_increment(do_volatile) &&
                         !has_snapshots();

        string smode        = backup_conf->vector_value("MODE");
        Backups::Mode bmode = Backups::str_to_mode(smode);

        if (!smode.empty() && !increment && bmode == Backups::INCREMENT)
        {
            err = "VM cannot use backup increment mode";

            if (has_snapshots())
            {
                err += ", it has snapshots.";
            }
            else
            {
                err += ", it has disks snapshots or disks are not qcow2.";
            }

            NebulaLog::log("ONE", Log::ERROR, err);
            return -1;
        }

        backup_conf = nullptr;

        if ( _backups.parse(tmpl, increment, disks.backup_keep_last(do_volatile), append, err) != 0 )
        {
            NebulaLog::log("ONE", Log::ERROR, err);
            return -1;
        }
    }

    encrypt();

    if ( context_changed )
    {
        return 0;
    }

    return 1;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* VirtualMachine Disks Interface                                             */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::get_disk_images(string& error_str)
{
    vector<VectorAttribute *> adisks;
    vector<VectorAttribute *> acontext_disks;

    int num_context = user_obj_template->remove("CONTEXT", acontext_disks);
    user_obj_template->remove("DISK", adisks);

    obj_template->set(acontext_disks);
    obj_template->set(adisks);

    VectorAttribute * context = 0;

    if ( num_context > 0 )
    {
        context = acontext_disks[0];
    }

    // -------------------------------------------------------------------------
    // Deployment mode for the VM disks
    // -------------------------------------------------------------------------
    std::string tm_mad_sys;

    if ( user_obj_template->get("TM_MAD_SYSTEM", tm_mad_sys) == true )
    {
        user_obj_template->erase("TM_MAD_SYSTEM");

        obj_template->add("TM_MAD_SYSTEM", tm_mad_sys);
    }

    VectorAttribute* os = obj_template->get("OS");
    bool is_q35         = false;

    if (os)
    {
        string machine = os->vector_value("MACHINE");
        is_q35         = machine.find("q35") != std::string::npos;
    }

    return disks.get_images(oid, uid, tm_mad_sys, adisks, context, is_q35, error_str);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::release_disk_images(vector<Template *>& quotas,
                                         bool set_state)
{
    bool image_error;

    if ( set_state )
    {
        image_error = (state == ACTIVE && lcm_state != EPILOG) &&
                      state != PENDING && state != HOLD &&
                      state != CLONING && state != CLONING_FAILURE;
    }
    else
    {
        image_error = false;
    }

    disks.release_images(oid, image_error, quotas);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::set_up_attach_disk(VirtualMachineTemplate * tmpl, string& err)
{
    VectorAttribute * new_vdisk = tmpl->get("DISK");

    if ( new_vdisk == 0 )
    {
        err = "Internal error parsing DISK attribute";
        return -1;
    }

    new_vdisk = new_vdisk->clone();

    VectorAttribute * context = get_template_attribute("CONTEXT");

    VirtualMachineDisk * new_disk;

    // -------------------------------------------------------------------------
    // Deployment mode for the VM disks
    // -------------------------------------------------------------------------
    std::string tm_mad_sys;

    obj_template->get("TM_MAD_SYSTEM", tm_mad_sys);

    new_disk = disks.set_up_attach(oid, uid, get_cid(), new_vdisk, tm_mad_sys, context, err);

    if ( new_disk == 0 )
    {
        delete new_vdisk;
        return -1;
    }

    // -------------------------------------------------------------------------
    // Add new disk to template and set info in history before attaching
    // -------------------------------------------------------------------------
    set_vm_info();

    obj_template->set(new_disk->vector_attribute());

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* Disk save as interface                                                     */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::set_saveas_state()
{
    switch (state)
    {
        case ACTIVE:
            if (lcm_state != RUNNING)
            {
                return -1;
            }

            set_state(HOTPLUG_SAVEAS);
            break;

        case POWEROFF:
            set_state(ACTIVE);
            set_state(HOTPLUG_SAVEAS_POWEROFF);
            break;

        case SUSPENDED:
            set_state(ACTIVE);
            set_state(HOTPLUG_SAVEAS_SUSPENDED);
            break;

        case UNDEPLOYED:
            set_state(ACTIVE);
            set_state(HOTPLUG_SAVEAS_UNDEPLOYED);
            break;

        case STOPPED:
            set_state(ACTIVE);
            set_state(HOTPLUG_SAVEAS_STOPPED);
            break;

        default:
            return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::clear_saveas_state()
{
    switch (lcm_state)
    {
        case HOTPLUG_SAVEAS:
            set_state(RUNNING);
            break;

        case HOTPLUG_SAVEAS_POWEROFF:
            set_state(POWEROFF);
            set_state(LCM_INIT);
            break;

        case HOTPLUG_SAVEAS_SUSPENDED:
            set_state(SUSPENDED);
            set_state(LCM_INIT);
            break;

        case HOTPLUG_SAVEAS_UNDEPLOYED:
            set_state(UNDEPLOYED);
            set_state(LCM_INIT);
            break;

        case HOTPLUG_SAVEAS_STOPPED:
            set_state(STOPPED);
            set_state(LCM_INIT);
            break;

        default:
            return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* VirtualMachine Nic interface                                               */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::get_auto_network_leases(VirtualMachineTemplate * tmpl,
                                            string& estr)
{
    vector<VectorAttribute *> vnics;

    int nic_id;

    /* ---------------------------------------------------------------------- */
    /* Update auto NICs with the scheduling resulti                           */
    /* ---------------------------------------------------------------------- */
    tmpl->get("NIC", vnics);

    for (auto vattr : vnics)
    {
        vattr->vector_value("NIC_ID", nic_id);

        VirtualMachineNic * nic = get_nic(nic_id);

        if (!nic)
        {
            std::ostringstream oss;

            oss << "NIC_ID " << nic_id << " not found";
            estr = oss.str();

            return -1;
        }

        string network_id = nic->vector_value("NETWORK_ID");

        if (!nic->is_auto() || !network_id.empty())
        {
            std::ostringstream oss;

            oss << "NIC_ID " << nic_id << " not AUTO";
            estr = oss.str();

            return -1;
        }

        nic->replace("NETWORK_ID", vattr->vector_value("NETWORK_ID"));
    }

    /* ---------------------------------------------------------------------- */
    /* Get the network leases & security groups for NICs in auto mode         */
    /* ---------------------------------------------------------------------- */
    vector<VectorAttribute*> sgs;

    VectorAttribute * nic_default = obj_template->get("NIC_DEFAULT");

    if (nics.get_auto_network_leases(oid, uid, nic_default, sgs, estr) == -1)
    {
        return -1;
    }

    obj_template->set(sgs);

    /* ---------------------------------------------------------------------- */
    /* Generate the CONTEXT for NICs in auto mode                             */
    /* ---------------------------------------------------------------------- */
    VectorAttribute * context = obj_template->get("CONTEXT");

    if ( context == 0 )
    {
        return 0;
    }

    if ( generate_network_context(context, estr, true) != 0 )
    {
        return -1;
    }

    return 0;
}

int VirtualMachine::get_network_leases(string& estr)
{
    /* ---------------------------------------------------------------------- */
    /* Get the NIC attributes:                                                */
    /*   * NIC                                                                */
    /*   * NIC_ALIAS                                                          */
    /*   * PCI + TYPE = NIC                                                   */
    /* ---------------------------------------------------------------------- */
    vector<Attribute *> anics;
    vector<Attribute *> alias;

    user_obj_template->remove("NIC", anics);

    for (auto it = anics.begin(); it != anics.end(); )
    {
        if ( (*it)->type() != Attribute::VECTOR )
        {
            delete *it;
            it = anics.erase(it);
        }
        else
        {
            obj_template->set(*it);
            ++it;
        }
    }

    user_obj_template->remove("NIC_ALIAS", alias);

    for (auto it = alias.begin(); it != alias.end(); )
    {
        if ( (*it)->type() != Attribute::VECTOR )
        {
            delete *it;
            it = alias.erase(it);
        }
        else
        {
            obj_template->set(*it);
            anics.push_back(*it);
            ++it;
        }
    }

    vector<VectorAttribute *> pcis;

    get_template_attribute("PCI", pcis);

    for (auto vattr : pcis)
    {
        if ( vattr->vector_value("TYPE") == "NIC" )
        {
            anics.push_back(vattr);
        }
    }

    /* ---------------------------------------------------------------------- */
    /* Get the network leases & security groups for the NICs and PCIs         */
    /* ---------------------------------------------------------------------- */
    vector<VectorAttribute*> sgs;

    VectorAttribute * nic_default = obj_template->get("NIC_DEFAULT");

    if (nics.get_network_leases(oid, uid, anics, nic_default, sgs, estr) == -1)
    {
        return -1;
    }

    /* ---------------------------------------------------------------------- */
    /* Get the associated secutiry groups for NICs and PCI TYPE=NIC           */
    /* ---------------------------------------------------------------------- */

    obj_template->set(sgs);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::set_up_attach_nic(VirtualMachineTemplate * tmpl, string& err)
{
    bool is_pci = false;

    // -------------------------------------------------------------------------
    // Get the new NIC attribute from the template
    // -------------------------------------------------------------------------
    VectorAttribute * new_nic = tmpl->get("NIC");

    if (new_nic == nullptr)
    {
        new_nic = tmpl->get("NIC_ALIAS");

        if (new_nic == nullptr)
        {
            new_nic = tmpl->get("PCI");

            if ( new_nic != nullptr && new_nic->vector_value("TYPE") != "NIC" )
            {
                new_nic = nullptr;
            }

            is_pci = true;
        }
    }

    if ( new_nic == nullptr )
    {
        err = "Wrong format or missing NIC/NIC_ALIAS/PCI attribute";
        return -1;
    }

    // -------------------------------------------------------------------------
    // Setup PCI attribute
    // -------------------------------------------------------------------------
    std::unique_ptr<VectorAttribute> _new_nic(new_nic->clone());

    if ( is_pci )
    {
        std::vector<const VectorAttribute*> pcis;
        vector<VectorAttribute *> nodes;

        std::map<unsigned int, std::set<unsigned int>> palloc;

        int max_pci_id = -1;

        obj_template->get("PCI", pcis);

        for (const auto& pci: pcis)
        {
            int pci_id;
            unsigned int numa_node;

            pci->vector_value("PCI_ID", pci_id, -1);

            if (pci_id > max_pci_id)
            {
                max_pci_id = pci_id;
            }

            if (pci->vector_value("NUMA_NODE", numa_node) != -1)
            {
                unsigned int index;

                if ( pci->vector_value("VM_BUS_INDEX", index) != -1 )
                {
                    std::set<unsigned int>& ports = palloc[numa_node];
                    ports.insert(index);
                }
            }
        }

        _new_nic->replace("PCI_ID", max_pci_id + 1);

        bool numa = obj_template->get("NUMA_NODE", nodes) > 0;

        if (HostSharePCI::set_pci_address(_new_nic.get(),
                                          palloc,
                                          test_machine_type("q35"),
                                          numa) == -1)
        {
            return -1;
        }
    }

    // -------------------------------------------------------------------------
    // Setup nic for attachment
    // -------------------------------------------------------------------------
    vector<VectorAttribute *> sgs;

    VectorAttribute * nic_default = obj_template->get("NIC_DEFAULT");

    int rc = nics.set_up_attach_nic(oid, uid, get_cid(), _new_nic.get(),
                                    nic_default, sgs, err);

    if ( rc != 0 )
    {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Add new nic to template and set info in history before attaching
    // -------------------------------------------------------------------------
    set_vm_info();

    obj_template->set(_new_nic.release());

    for (auto vattr : sgs)
    {
        obj_template->set(vattr);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::set_detach_nic(int nic_id)
{
    std::set<int> a_ids;

    int parent_id, alias_id;

    VirtualMachineNic * nic = nics.get_nic(nic_id);

    if ( nic == 0 )
    {
        return -1;
    }

    nic->set_attach();

    if ( nic->is_alias() )
    {
        std::ostringstream oss;

        if ( nic->vector_value("ALIAS_ID", alias_id) != 0 ||
             nic->vector_value("PARENT_ID", parent_id) != 0 )
        {
            return -1;
        }

        nic = nics.get_nic(parent_id);

        if ( nic == 0 )
        {
            return -1;
        }

        one_util::split_unique(nic->vector_value("ALIAS_IDS"), ',', a_ids);

        for (const auto& id : a_ids)
        {
            if ( id == nic_id )
            {
                continue;
            }

            if ( !oss.str().empty() )
            {
                oss << ",";
            }

            oss << id;
        }

        nic->replace("ALIAS_IDS", oss.str());
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::delete_attach_alias(VirtualMachineNic *nic)
{
    std::set<int> a_ids;

    one_util::split_unique(nic->vector_value("ALIAS_IDS"), ',', a_ids);

    for (auto id : a_ids)
    {
        VirtualMachineNic * nic_a = nics.delete_nic(id);

        if (nic_a)
        {
            obj_template->remove(nic_a->vector_attribute());
        }
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::attach_pci(VectorAttribute * vpci, string& err)
{
    std::vector<const VectorAttribute*> pcis;

    std::map<unsigned int, std::set<unsigned int>> palloc;

    int max_pci_id = -1;

    obj_template->get("PCI", pcis);

    for (const auto& pci: pcis)
    {
        int pci_id;

        unsigned int numa_node;

        pci->vector_value("PCI_ID", pci_id, -1);

        if (pci_id > max_pci_id)
        {
            max_pci_id = pci_id;
        }

        if (pci->vector_value("NUMA_NODE", numa_node) != -1)
        {
            unsigned int index;

            if ( pci->vector_value("VM_BUS_INDEX", index) != -1 )
            {
                std::set<unsigned int>& ports = palloc[numa_node];
                ports.insert(index);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Setup PCI attribute & Context (VM running at host and PCI is allocated)
    // -------------------------------------------------------------------------
    std::unique_ptr<VectorAttribute> _new_pci(vpci->clone());

    _new_pci->replace("PCI_ID", max_pci_id + 1);

    add_pci_context(_new_pci.get());

    vector<VectorAttribute *> nodes;

    bool numa = obj_template->get("NUMA_NODE", nodes) > 0;

    if (HostSharePCI::set_pci_address(_new_pci.get(),
                                      palloc,
                                      test_machine_type("q35"),
                                      numa) == -1)
    {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Add new nic to template
    // -------------------------------------------------------------------------
    obj_template->set(_new_pci.release());

    return max_pci_id + 1;
}

// -----------------------------------------------------------------------------

VectorAttribute * VirtualMachine::get_pci(int pci_id)
{
    std::vector<VectorAttribute*> pcis;

    obj_template->get("PCI", pcis);

    for (auto& pci: pcis)
    {
        int id;

        pci->vector_value("PCI_ID", id, -1);

        if (pci_id == id)
        {
            return pci;
        }
    }

    return nullptr;
}

// -----------------------------------------------------------------------------

void VirtualMachine::detach_pci(VectorAttribute * pci)
{
    // -------------------------------------------------------------------------
    // Remove from Template & Context
    // -------------------------------------------------------------------------
    clear_pci_context(pci);

    obj_template->remove(pci);

    delete pci;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* VirtualMachine VMGroup interface                                           */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int VirtualMachine::get_vmgroup(string& error)
{
    vector<Attribute  *> vmgroups;

    bool found = false;
    VectorAttribute * thegroup = 0;

    user_obj_template->remove("VMGROUP", vmgroups);

    for (auto it = vmgroups.begin(); it != vmgroups.end(); )
    {
        if ( (*it)->type() != Attribute::VECTOR || found )
        {
            delete *it;
            it = vmgroups.erase(it);
        }
        else
        {
            thegroup = dynamic_cast<VectorAttribute *>(*it);
            found    = true;

            ++it;
        }
    }

    if ( thegroup == 0 )
    {
        return 0;
    }

    VMGroupPool * vmgrouppool = Nebula::instance().get_vmgrouppool();
    int rc;

    rc = vmgrouppool->vmgroup_attribute(thegroup, get_uid(), get_oid(), error);

    if ( rc != 0 )
    {
        delete thegroup;

        return -1;
    }

    obj_template->set(thegroup);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::release_vmgroup()
{
    VectorAttribute * thegroup = obj_template->get("VMGROUP");

    if ( thegroup == 0 )
    {
        return;
    }

    VMGroupPool * vmgrouppool = Nebula::instance().get_vmgrouppool();

    vmgrouppool->del_vm(thegroup, get_oid());
}

/* -------------------------------------------------------------------------- */

int VirtualMachine::vmgroup_id()
{
    int vmg_id;

    VectorAttribute * thegroup = obj_template->get("VMGROUP");

    if ( thegroup == nullptr )
    {
        return -1;
    }

    if ( thegroup->vector_value("VMGROUP_ID", vmg_id) == 0 )
    {
        return vmg_id;
    }

    return -1;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

int VirtualMachine::check_tm_mad_disks(const string& tm_mad, string& error)
{
    string tm_mad_sys;

    obj_template->get("TM_MAD_SYSTEM", tm_mad_sys);

    if ( !tm_mad_sys.empty() ) // VM has TM_MAD_SYSTEM already defined
    {
        return 0;
    }
    if ( disks.check_tm_mad(tm_mad, error) != 0 )
    {
        return -1;
    }

    obj_template->add("TM_MAD_SYSTEM", tm_mad);

    return 0;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

int VirtualMachine::check_shareable_disks(const string& vmm_mad, string& error)
{
    VirtualMachineManager * vmm = Nebula::instance().get_vmm();
    const VirtualMachineManagerDriver * vmmd = vmm->get(vmm_mad);

    if ( vmmd == nullptr )
    {
        error = "Cannot find vmm driver: " + vmm_mad;

        return -1;
    }

    if ( vmmd->support_shareable() )
    {
        return 0;
    }

    for (const auto disk : disks)
    {
        bool shareable;
        disk->vector_value("SHAREABLE", shareable);

        if (shareable)
        {
            error = "VM has shareable disk, but vmm driver: '"
                    + vmm_mad + "' doesn't support shareable disks";

            return -1;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

void VirtualMachine::encrypt()
{
    std::string one_key;
    Nebula::instance().get_configuration_attribute("ONE_KEY", one_key);

    obj_template->encrypt(one_key);
    user_obj_template->encrypt(one_key);
};

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

void VirtualMachine::decrypt()
{
    std::string one_key;
    Nebula::instance().get_configuration_attribute("ONE_KEY", one_key);

    obj_template->decrypt(one_key);
    user_obj_template->decrypt(one_key);
};

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

bool VirtualMachine::is_running_quota() const
{
    return (state == VirtualMachine::PENDING) ||
           (state == VirtualMachine::CLONING) ||
           (state == VirtualMachine::CLONING_FAILURE) ||
           (state == VirtualMachine::HOLD) ||
           ((state == VirtualMachine::ACTIVE &&
             (lcm_state != VirtualMachine::HOTPLUG_SAVEAS_POWEROFF &&
              lcm_state != VirtualMachine::HOTPLUG_SAVEAS_SUSPENDED &&
              lcm_state != VirtualMachine::DISK_SNAPSHOT_POWEROFF &&
              lcm_state != VirtualMachine::DISK_SNAPSHOT_REVERT_POWEROFF &&
              lcm_state != VirtualMachine::DISK_SNAPSHOT_DELETE_POWEROFF &&
              lcm_state != VirtualMachine::DISK_SNAPSHOT_SUSPENDED &&
              lcm_state != VirtualMachine::DISK_SNAPSHOT_DELETE_SUSPENDED &&
              lcm_state != VirtualMachine::DISK_RESIZE_POWEROFF &&
              lcm_state != VirtualMachine::DISK_RESIZE_UNDEPLOYED &&
              lcm_state != VirtualMachine::HOTPLUG_NIC_POWEROFF &&
              lcm_state != VirtualMachine::HOTPLUG_SAVEAS_UNDEPLOYED &&
              lcm_state != VirtualMachine::HOTPLUG_SAVEAS_STOPPED &&
              lcm_state != VirtualMachine::HOTPLUG_PROLOG_POWEROFF &&
              lcm_state != VirtualMachine::HOTPLUG_EPILOG_POWEROFF )));
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

void VirtualMachine::get_quota_template(VirtualMachineTemplate& quota_tmpl,
                                        bool basic_quota, bool running_quota)
{
    if (hasHistory())
    {
        quota_tmpl.replace("CLUSTER_ID", get_cid());
    }

    if (basic_quota)
    {
        quota_tmpl.replace("VMS", 1);

        for (const string& metric : QuotaVirtualMachine::generic_metrics())
        {
            string value;

            // Use value from user template, if it's not already added from template
            if (user_obj_template->get(metric, value))
            {
                quota_tmpl.replace(metric, value);
            }
        }

        quota_tmpl.merge(obj_template.get());
    }

    if (running_quota)
    {
        string memory, cpu;

        get_template_attribute("MEMORY", memory);
        get_template_attribute("CPU", cpu);

        quota_tmpl.add("RUNNING_MEMORY", memory);
        quota_tmpl.add("RUNNING_CPU", cpu);
        quota_tmpl.add("RUNNING_VMS", 1);

        for (const string& metric : QuotaVirtualMachine::generic_metrics())
        {
            string value;
            if (obj_template->get(metric, value))
            {
                quota_tmpl.add("RUNNING_" + metric, value);
            }
            else if (user_obj_template->get(metric, value))
            {
                quota_tmpl.add("RUNNING_" + metric, value);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::release_vnc_port()
{
    if (!hasHistory())
    {
        return;
    }

    ClusterPool * cpool = Nebula::instance().get_clpool();

    VectorAttribute * graphics = get_template_attribute("GRAPHICS");

    unsigned int port;

    if (graphics == nullptr ||
        graphics->vector_value("PORT", port) != 0)
    {
        return;
    }

    cpool->release_vnc_port(get_cid(), port);

    graphics->remove("PORT");
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachine::release_previous_vnc_port()
{
    if (!hasPreviousHistory())
    {
        return;
    }

    ClusterPool * cpool = Nebula::instance().get_clpool();

    VectorAttribute * graphics = get_template_attribute("GRAPHICS");

    unsigned int previous_port;

    if (graphics == nullptr ||
        graphics->vector_value("PREVIOUS_PORT", previous_port) != 0)
    {
        return;
    }

    cpool->release_vnc_port(previous_history->cid, previous_port);

    graphics->remove("PREVIOUS_PORT");
};

/* -------------------------------------------------------------------------- */

void VirtualMachine::rollback_previous_vnc_port()
{
    ClusterPool * cpool = Nebula::instance().get_clpool();

    VectorAttribute * graphics = get_template_attribute("GRAPHICS");

    unsigned int previous_port;
    unsigned int port;

    if (graphics == nullptr ||
        graphics->vector_value("PREVIOUS_PORT", previous_port) != 0)
    {
        return;
    }

    if ( graphics->vector_value("PORT", port) == 0 )
    {
        cpool->release_vnc_port(history->cid, port);
    }

    graphics->replace("PORT", previous_port);

    graphics->remove("PREVIOUS_PORT");
};

