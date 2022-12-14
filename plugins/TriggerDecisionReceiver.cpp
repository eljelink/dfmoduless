/**
 * @file TriggerDecisionReceiver.cpp TriggerDecisionReceiver class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TriggerDecisionReceiver.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "dfmodules/triggerdecisionreceiver/Nljs.hpp"
#include "dfmodules/triggerdecisionreceiver/Structs.hpp"
#include "dfmodules/triggerdecisionreceiverinfo/InfoNljs.hpp"
#include "logging/Logging.hpp"
#include "iomanager/IOManager.hpp"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "TriggerDecisionReceiver" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

TriggerDecisionReceiver::TriggerDecisionReceiver(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_queue_timeout(100)
  , m_run_number(0)
{
  register_command("conf", &TriggerDecisionReceiver::do_conf);
  register_command("start", &TriggerDecisionReceiver::do_start);
  register_command("stop", &TriggerDecisionReceiver::do_stop);
  register_command("scrap", &TriggerDecisionReceiver::do_scrap);
}

void
TriggerDecisionReceiver::init(const data_t& data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  auto qi = appfwk::connection_index(data, { "input", "output" });

  m_input_connection = qi["input"];
  m_triggerdecision_output = iomanager::IOManager::get()->get_sender<dfmessages::TriggerDecision>( qi["output"] );

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
TriggerDecisionReceiver::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";
  triggerdecisionreceiver::ConfParams parsed_conf = payload.get<triggerdecisionreceiver::ConfParams>();

  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
TriggerDecisionReceiver::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_received_triggerdecisions = 0;
  m_run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);

  iomanager::IOManager::get()->add_callback<dfmessages::TriggerDecision>( m_input_connection,
									  std::bind(&TriggerDecisionReceiver::dispatch_triggerdecision, this, std::placeholders::_1));

  TLOG() << get_name() << " successfully started for run number " << m_run_number;
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
TriggerDecisionReceiver::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  iomanager::IOManager::get()->remove_callback<dfmessages::TriggerDecision>( m_input_connection );

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
TriggerDecisionReceiver::do_scrap(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  TLOG() << get_name() << " successfully scrapped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
TriggerDecisionReceiver::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  triggerdecisionreceiverinfo::Info info;
  info.triggerdecisions_received = m_received_triggerdecisions;
  ci.add(info);
}

void
TriggerDecisionReceiver::dispatch_triggerdecision(dfmessages::TriggerDecision & td)
{
  m_triggerdecision_output->send(std::move(td), m_queue_timeout);
  m_received_triggerdecisions++;
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::TriggerDecisionReceiver)
