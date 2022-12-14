/**
 * @file TriggerInhibitAgent.cpp TriggerInhibitAgent class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/TriggerInhibitAgent.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "logging/Logging.hpp"

#include <memory>
#include <string>
#include <utility>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "TriggerInhibitAgent" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

TriggerInhibitAgent::TriggerInhibitAgent(const std::string& parent_name,
                                         std::shared_ptr<trigdecreceiver_t> our_input,
                                         std::shared_ptr<triginhsender_t> our_output)
  : NamedObject(parent_name + "::TriggerInhibitAgent")
  , m_thread(std::bind(&TriggerInhibitAgent::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_threshold_for_inhibit(1)
  , m_trigger_decision_receiver(our_input)
  , m_trigger_inhibit_sender(our_output)
  , m_trigger_number_at_start_of_processing_chain(0)
  , m_trigger_number_at_end_of_processing_chain(0)
{}

void
TriggerInhibitAgent::start_checking()
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering start_checking() method";
  m_thread.start_working_thread();
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting start_checking() method";
}

void
TriggerInhibitAgent::stop_checking()
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering stop_checking() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting stop_checking() method";
}

void
TriggerInhibitAgent::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";

  // configuration (hard-coded, for now; will be input from calling code later)
  int fake_busy_interval_sec = 0;
  std::chrono::seconds chrono_fake_busy_interval(fake_busy_interval_sec);
  int fake_busy_duration_sec = 0;
  std::chrono::seconds chrono_fake_busy_duration(fake_busy_duration_sec);
  int min_interval_between_inhibit_messages_msec = 0;
  std::chrono::milliseconds chrono_min_interval_between_inhibit_messages(min_interval_between_inhibit_messages_msec);

  // initialization
  enum LocalState
  {
    no_update,
    free_state,
    busy_state
  };
  std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
  // std::chrono::steady_clock::time_point start_time_of_latest_fake_busy = current_time - chrono_fake_busy_duration;
  std::chrono::steady_clock::time_point last_sent_time = current_time;
  LocalState requested_state = no_update;
  LocalState current_state = free_state;
  int32_t received_message_count = 0;
  int32_t sent_message_count = 0;

  // work loop
  while (running_flag.load()) {

    // check if a TriggerDecision message has arrived, and save the trigger
    // number contained within it, if one has arrived
    try {
      dfmessages::TriggerDecision trig_dec = m_trigger_decision_receiver->receive(m_queue_timeout);
      ++received_message_count;
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Popped the TriggerDecision for trigger number "
                                  << trig_dec.trigger_number << " off the input queue";
      m_trigger_number_at_start_of_processing_chain.store(trig_dec.trigger_number);
    } catch (const iomanager::TimeoutExpired& excpt) {
      // it is perfectly reasonable that there will be no data in the queue some
      // fraction of the times that we check, so we just continue on and try again later
    }

    // to-do: add some logic to fake inhibits

    // check if A) we are supposed to be checking the trigger_number difference, and
    // B) if so, whether an Inhibit should be asserted or cleared
    uint32_t threshold = m_threshold_for_inhibit.load(); // NOLINT
    if (threshold > 0) {
      daqdataformats::trigger_number_t temp_trig_num_at_start = m_trigger_number_at_start_of_processing_chain.load();
      daqdataformats::trigger_number_t temp_trig_num_at_end = m_trigger_number_at_end_of_processing_chain.load();
      if (temp_trig_num_at_start >= temp_trig_num_at_end &&
          (temp_trig_num_at_start - temp_trig_num_at_end) >= threshold) {
        if (current_state == free_state) {
          requested_state = busy_state;
        }
      } else {
        if (current_state == busy_state) {
          requested_state = free_state;
        }
      }
    }

    // to-do: add some logic to periodically send a message even if nothing has changed

    // send an Inhibit messages, if needed (either Busy or Free state)
    if (requested_state != no_update && requested_state != current_state) {
      if ((std::chrono::steady_clock::now() - last_sent_time) >= chrono_min_interval_between_inhibit_messages) {
        dfmessages::TriggerInhibit inhibit_message;
        if (requested_state == busy_state) {
          inhibit_message.busy = true;
        } else {
          inhibit_message.busy = false;
        }

        TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Pushing a TriggerInhibit message with busy state set to "
                                    << inhibit_message.busy << " onto the output queue";
        try {
          m_trigger_inhibit_sender->send(std::move(inhibit_message), m_queue_timeout);
          ++sent_message_count;
#if 0
          // temporary logging
          std::ostringstream oss_sent;
          oss_sent << ": Successfully pushed a TriggerInhibit message with busy state set to " << inhibit_message.busy
                   << " onto the output queue";
          TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_sent.str());
#endif
          // if we successfully pushed the message to the Sink, then we assume that the
          // receiver will get it, and we update our internal state accordingly
          current_state = requested_state;
          requested_state = no_update;
          last_sent_time = std::chrono::steady_clock::now();
        } catch (const iomanager::TimeoutExpired& excpt) {
          // It is not ideal if we fail to send the inhibit message out, but rather than
          // retrying some unknown number of times, we simply output a TRACE message and
          // go on.  This has the benefit of being responsive with pulling TriggerDecision
          // messages off the input queue, and maybe our Busy/Free state will have changed
          // by the time that the receiver is ready to receive more messages.
          TLOG_DEBUG(TLVL_WORK_STEPS) << get_name()
                                      << ": TIMEOUT pushing a TriggerInhibit message onto the output queue";
        }
      }
    }
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received " << received_message_count
           << " TriggerDecision messages and sent " << sent_message_count
           << " TriggerInhibit messages of all types (both Busy and Free).";
  TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_summ.str());
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq
