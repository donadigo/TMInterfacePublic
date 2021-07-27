/* Copyright Adam Bieñkowski <donadigos159@gmail.com> - All Rights Reserved
 * Unauthorized redistribution and copying of this software and its source code via any medium is strictly prohibited.
 * Proprietary and confidential.
 */

#include "pch.h"
#include "BFSearchController.h"
#include "InputMutation.h"
#include "InterfaceConsole.h"
#include "Utils.h"
#include "TMInterfaceImpl.h"
#include <filesystem>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <fmt/format.h>
#include <fmt/color.h>

static const unsigned kMaxNeighbourCalls = 200;
static const fmt::color kSuccessColor = fmt::color::light_green;
static const fmt::color kWarningColor = fmt::color::yellow;
static const fmt::color kErrorColor = fmt::color::red;
static const fmt::color kInfoColor = fmt::color::white;

template<typename T>
static inline bool IndexInBounds(const std::vector<T>& vec, const int index)
{
	return index >= 0 && index < static_cast<int>(vec.size());
}

BFSearchController::BFSearchController()
{
}

void BFSearchController::OnSimulationBegin(SimulationManager& simManager)
{
	auto console = CONSOLE;
	m_simulating = console->GetValue<std::string>("controller") == "bruteforce";
	if (!m_simulating) {
		return;
	}

	m_origStates.clear();
	m_iterations = 0;
	m_improvements = 0;

	m_settingSimDebug = console->GetValue<bool>("sim_debug");
	UpdateSettings();

	CTrackMania* app = address::GetApp();
	if (app && app->SystemOverlay && app->Viewport && app->Viewport->SystemWindow && app->Viewport->SystemWindow->IsFullscreen) {
		app->SystemOverlay->GoWindowed();
	}

	utils::EnsureConsoleAllocated();

	fmt::print(fmt::fg(kInfoColor),
			   "Bruteforce script started. To stop the script, press the Escape key on your keyboard.\n"
			   "Each line represents a new run finished. If the run did not finish, the time equals -1.\n"
			   "The number of modified inputs for each try can be seen in the brackets.\n\n"
			   "If the simulation ends immediately with result \"Wrong Simulation\", make sure to export the replay for validation first.\n\n");

	if (m_settingModifyChance <= 0) {
		fmt::print(fmt::fg(kErrorColor),
				   "bf_modify_chance is set to {} which prevents the script from changing inputs.\n" 
				   "Make sure to set bf_modify_chance to a non-zero positive value between 0 and 1.\n\n",
				   m_settingModifyChance);
		m_simulating = false;
		return;
	}

	if (m_settingMaxTimeDiff <= 0 && m_settingMaxSteerDiff <= 0) {
		fmt::print(fmt::fg(kErrorColor),
				   "bf_max_time_diff is set to {} and bf_max_steer_diff is set to {} which prevents the script from changing inputs.\n"
				   "Make sure to set either bf_max_time_diff or bf_max_steer_diff to a non-zero positive value.\n\n",
				   m_settingMaxTimeDiff, m_settingMaxSteerDiff);
		m_simulating = false;
		return;
	}

	simManager.RemoveStateValidation();
	StartInitialPhase();

	if (m_userTargetCheckpoint >= 0) {
		fmt::print(fmt::fg(kInfoColor), "Target: checkpoint {} time\n", m_userTargetCheckpoint + 1);
		m_userTarget = BFTarget::kCheckpointTime;
	} else if (m_userTargetTrigger >= 0) {
		fmt::print(fmt::fg(kInfoColor), "Target: trigger {} time/distance\n", m_userTargetTrigger + 1);
		m_userTarget = BFTarget::kTrigger;
	} else {
		fmt::print(fmt::fg(kInfoColor), "Target: finish time\n");
		m_userTarget = BFTarget::kFinishTime;
	}

	auto buffer = simManager.GetEventBuffer();
	if (buffer) {
		m_controlNames = buffer->GetControlNamesData();
		m_eventsDuration = buffer->EventsDuration;

		if (m_settingInputsSteerFill) {
			input_mutation::ExtendSteerInputs(buffer, m_controlNames);
			fmt::print(fmt::fg(kInfoColor), "Using extended steer inputs\n");
		}
	}
}

void BFSearchController::UpdateSettings()
{
	auto console = CONSOLE;
	m_settingPrintCps			= console->GetValue<bool>("bf_print_cps");
	m_settingSearchForever		= console->GetValue<bool>("bf_search_forever");
	m_settingModifyChance		= console->GetValue<double>("bf_modify_prob");
	//m_settingModifyNumber		= static_cast<int>(console->GetValue<double>("bf_modify_number");
	m_settingMaxSteerDiff		= static_cast<int>(console->GetValue<double>("bf_max_steer_diff"));
	m_settingMaxTimeDiff		= static_cast<int>(console->GetValue<double>("bf_max_time_diff"));
	m_settingOverrideStopTime	= static_cast<int>(console->GetValue<double>("bf_override_stop_time"));

	m_settingInputsMinTime		= static_cast<int>(console->GetValue<double>("bf_inputs_min_time"));
	m_settingInputsMinTime		= m_settingInputsMinTime <= 0 ? 0 : m_settingInputsMinTime;

	m_settingInputsMaxTime		= static_cast<int>(console->GetValue<double>("bf_inputs_max_time"));
	m_settingInputsMaxTime		= m_settingInputsMaxTime <= 0 ? (MAX_TIME - 100010) : m_settingInputsMaxTime;

	m_settingInputsSteerFill	= console->GetValue<bool>("bf_inputs_fill_steer");

	m_userTargetCheckpoint		= static_cast<int>(console->GetValue<double>("bf_target_cp")) - 1;
	m_userTargetTrigger			= static_cast<int>(console->GetValue<double>("bf_target_trigger")) - 1;
}

static bool ShouldStop()
{
	return (GetAsyncKeyState(VK_ESCAPE) & 0x01) && GetForegroundWindow() == GetConsoleWindow();
}

void BFSearchController::OnSimulationEnd(SimulationManager& simManager, const uint32_t result)
{
	m_simulating = false;
}

void BFSearchController::OnSimulationStep(SimulationManager& simManager, const int time)
{
	if (!m_simulating) {
		return;
	}

	if (ShouldStop()) {
		fmt::print(fmt::fg(kInfoColor), "Escape key pressed, bruteforce script stopped.\n\n");
		m_simulating = false;
		return;
	}

	if (m_settingSimDebug) {
		UpdateSettings();
	}

	auto pos = simManager.GetCarPosition();
	auto vel = simManager.GetCarVelocity();

	int raceTime = time - 2610;
	bool doReturn = false;
	if (m_phase == BFPhase::kInitial) {
		ProcessTriggers(simManager, simManager.GetRaceTime(), pos, vel, m_targetTriggerHits);
		m_prevPos = pos;

		if (time >= 2610) {
			auto state = simManager.SaveState(ContextMode::kSimulation);
			if (state) {
				m_origStates.push_back(std::move(*state));
			}
		}

		if (time == 2600) {
			ReadInputsFromBuffer(simManager, m_originalInputs);
			m_currentInputs = m_originalInputs;
		}

		doReturn = true;
	}

	if (raceTime > m_eventsDuration && m_phase == BFPhase::kInitial) {
		// At this point we did not finish in the initial phase, which means that the user target is not a finish,
		// or the replay does not have valid physics
		// Start the search phase
		StartSearchPhase(simManager);
	}

	if (AskClientForEvaluation(simManager, time) || doReturn) {
		return;
	}

	if (raceTime > m_eventsDuration && m_phase == BFPhase::kSearch) {
		StartNewIteration(simManager);
	}

	EvaluateAll(simManager, raceTime);

	// TODO: this should be retrieved from m_states
	// We might have already rewinded to some state, retrieve the current position again
	m_prevPos = simManager.GetCarPosition();
}

void BFSearchController::EvaluateAll(SimulationManager& simManager, const int raceTime)
{
	auto pos = simManager.GetCarPosition();
	auto vel = simManager.GetCarVelocity();

	if (m_userTarget == BFTarget::kFinishTime) {
		if (!m_currentCpTimes.empty() && m_currentCpTimes.size() >= m_targetCpTimes.size() && m_currentCpTimes.back() < m_bestTime) {
			PrintSaveInputs(simManager, m_currentCpTimes.back());
			fmt::print(
				fmt::fg(kSuccessColor),
				"Found lower finish time: {} (-{}), iterations: {}\n",
				utils::FormatRaceTime(m_currentCpTimes.back()),
				utils::FormatRaceTime(m_bestTime - m_currentCpTimes.back()),
				m_iterations
			);

			Sleep(1000);

			if (m_settingSearchForever) {
				ResetWithNewSolution(simManager);
				return;
			} else {
				m_simulating = false;
			}

			StartNewIteration(simManager);
		} else if ((!m_targetCpTimes.empty() && raceTime > m_targetCpTimes.back()) || raceTime > m_eventsDuration) {
			StartNewIteration(simManager, -1, true);
		}

		return;
	}

	bool pastOverrideTime = m_settingOverrideStopTime > 0 && raceTime > m_settingOverrideStopTime;
	if (m_userTarget == BFTarget::kCheckpointTime) {
		if (IndexInBounds(m_currentCpTimes, m_userTargetCheckpoint) &&
			m_currentCpTimes[m_userTargetCheckpoint] < m_targetCpTimes[m_userTargetCheckpoint]) {
			PrintSaveInputs(simManager, m_currentCpTimes.back());
			fmt::print(
				fmt::fg(kSuccessColor), "Found lower checkpoint {}, time: {}, iterations: {}\n",
				m_userTargetCheckpoint + 1,
				utils::FormatRaceTime(m_currentCpTimes[m_userTargetCheckpoint]),
				m_iterations
			);

			Sleep(1000);

			if (m_settingSearchForever) {
				ResetWithNewSolution(simManager);
				return;
			} else {
				m_simulating = false;
			}

			StartNewIteration(simManager);
		} else if ((IndexInBounds(m_targetCpTimes, m_userTargetCheckpoint) && raceTime > m_targetCpTimes[m_userTargetCheckpoint]) || pastOverrideTime) {
			StartNewIteration(simManager, -1, true);
		}

		return;
	}

	if (m_userTarget == BFTarget::kTrigger) {
		ProcessTriggers(simManager, simManager.GetRaceTime(), pos, vel, m_currentTriggerHits);
		if (IndexInBounds(m_currentTriggerHits, m_userTargetTrigger) &&
			m_currentTriggerHits[m_userTargetTrigger].Compare(m_targetTriggerHits[m_userTargetTrigger], false)) {

			PrintSaveInputs(simManager, m_currentTriggerHits[m_userTargetTrigger].time);
			fmt::print(
				fmt::fg(kSuccessColor),
				"Found better trigger: {} {} < {}, iterations: {}\n",
				m_userTargetTrigger,
				m_currentTriggerHits[m_userTargetTrigger],
				m_targetTriggerHits[m_userTargetTrigger],
				m_iterations
			);

			Sleep(1000);

			if (m_settingSearchForever) {
				ResetWithNewSolution(simManager);
				return;
			} else {
				m_simulating = false;
			}

			StartNewIteration(simManager);
		} else if ((IndexInBounds(m_targetTriggerHits, m_userTargetTrigger) && raceTime > m_targetTriggerHits[m_userTargetTrigger].time) || pastOverrideTime) {
			StartNewIteration(simManager, -1, true);
		}
	}
}

void BFSearchController::ResetWithNewSolution(SimulationManager& simManager)
{
	StartInitialPhase();
	ReadInputsFromBuffer(simManager, m_originalInputs);
	m_currentInputs = m_originalInputs;

	simManager.RewindToState(m_origStates.front());

	// We are in kInitial again so clear all states but the first one
	m_origStates.resize(1);
}

void BFSearchController::StartInitialPhase()
{
	m_modified = 0;
	m_targetCpTimes.clear();
	m_currentCpTimes.clear();
	m_currentTriggerHits.clear();
	m_targetTriggerHits.clear();
	m_currentTrigger = nullptr;
	m_phase = BFPhase::kInitial;
	DebugLog("Start initial phase");
}

void BFSearchController::StartNewIteration(SimulationManager& simManager, int rewindTime, const bool printIteration)
{
	if (printIteration) {
		if (m_iterations % 1000 == 0 && m_iterations > 0) {
			fmt::print(fmt::fg(kInfoColor), "Iterations: {}\n", m_iterations);
		}

		fmt::print("-1 ({})\n", m_modified);
	}

	m_currentTriggerHits.clear();
	m_currentTrigger = nullptr;

	if (rewindTime < 0) {
		rewindTime = RandomNeighbour(simManager);
	}

	int rewindIndex = rewindTime / 10;
	if (!IndexInBounds(m_origStates, rewindIndex)) {
		return;
	}

	DebugLog("Rewind time: {}", rewindTime);
	simManager.RewindToState(m_origStates[rewindIndex]);

	// TODO: just use what game stores
	m_currentCpTimes.clear();
	CTrackManiaPlayerInfo* pInfo = simManager.GetPlayerInfo();
	if (pInfo) {
		CFastBuffer<TMCheckpointTime> cpTimes = *pInfo->GetCheckpointTimes();
		for (int i = 0; i < cpTimes.size; i++) {
			if (cpTimes[i].Time != UINT32_MAX) {
				m_currentCpTimes.push_back(cpTimes[i].Time);
			}
		}
	}

	m_iterations++;
}

void BFSearchController::OnCheckpointCountChanged(SimulationManager& simManager)
{
	if (!m_simulating) {
		return;
	}

	int raceTime = simManager.GetRaceTime();
	if (simManager.GetFinished()) {
		fmt::print("{} ({})\n", raceTime, m_modified);

		if (m_phase == BFPhase::kInitial) {
			if (m_settingOverrideStopTime > 0 && m_improvements == 0) {
				m_targetCpTimes.push_back(m_settingOverrideStopTime);
			} else {
				m_targetCpTimes.push_back(raceTime);
			}

			if (m_settingPrintCps) {
				fmt::print(fmt::fg(kInfoColor), "Initial phase finish: {}\n", m_targetCpTimes.back());
			}

			StartSearchPhase(simManager);
		} else {
			m_currentCpTimes.push_back(raceTime);
		}

		simManager.PreventSimulationFinish();
	} else if (m_phase == BFPhase::kInitial) {
		m_targetCpTimes.push_back(raceTime);
		fmt::print(fmt::fg(kInfoColor), "Initial phase checkpoint: {}\n", m_targetCpTimes.back());
	} else if (m_phase == BFPhase::kSearch) {
		m_currentCpTimes.push_back(raceTime);

		if (m_settingPrintCps) {
			fmt::print(fmt::fg(kInfoColor), "Checkpoint: {}\n", m_currentCpTimes.back());
		}
	}
}

void BFSearchController::StartSearchPhase(SimulationManager& simManager)
{
	if (m_userTarget == BFTarget::kTrigger && IndexInBounds(m_targetTriggerHits, m_userTargetTrigger)) {
		m_bestTime = m_targetTriggerHits[m_userTargetTrigger].time;
	} else if (!m_targetCpTimes.empty()) {
		if (m_userTarget == BFTarget::kFinishTime) {
			m_bestTime = m_targetCpTimes.back();
		} else if (IndexInBounds(m_targetCpTimes, m_userTargetCheckpoint)) {
			m_bestTime = m_targetCpTimes[m_userTargetCheckpoint];
		}
	}

	m_currentTrigger = nullptr;
	auto triggerManager = simManager.GetControllerByType<TriggerManager>();
	size_t triggerCount = triggerManager->Size();
	if (m_targetTriggerHits.size() < triggerCount) {
		for (size_t i = 0; i < triggerCount - m_targetTriggerHits.size(); i++) {
			m_targetTriggerHits.emplace_back(m_bestTime, 0);
		}
	}

	//PopulateSteerSequences();

	m_phase = BFPhase::kSearch;
	RandomNeighbour(simManager);
}

void BFSearchController::ReadInputsFromBuffer(SimulationManager& simManager, std::vector<TMEvent>& inputs)
{
	TMEventBuffer* eventBuffer = simManager.GetEventBuffer();
	if (!eventBuffer) {
		return;
	}

	inputs.clear();
	inputs.reserve(eventBuffer->EventsStore.Events.size);
	for (int i = 0; i < eventBuffer->EventsStore.Events.size; i++) {
		inputs.push_back(eventBuffer->EventsStore.Events.data[i + eventBuffer->EventsStore.Offset]);
	}
}

int BFSearchController::RandomNeighbour(SimulationManager& simManager, const unsigned nCall)
{
	if (nCall == kMaxNeighbourCalls) {
		fmt::print(fmt::fg(kWarningColor),
				   "[Warning] Exceeded limit of recursive calls for generating a new solution! "
				   "Make sure that your bruteforce settings allow for changing the inputs of this replay.\n");
		Sleep(1000);
		return -1;
	}

	m_modified = 0;
	TMEventBuffer* eventBuffer = simManager.GetEventBuffer();
	if (!eventBuffer) {
		return -1;
	}

	int minTime = MAX_TIME;
	int upperBound = (std::min)(100010 + m_settingInputsMaxTime, 100010 + m_bestTime);

	for (int i = 0; i < eventBuffer->EventsStore.Events.size; i++) {
		TMEvent& ev = eventBuffer->EventsStore.Events.data[i + eventBuffer->EventsStore.Offset];
		ev.Data = m_currentInputs[i].Data;
		ev.Time = m_currentInputs[i].Time;
	}

	for (int i = 0; i < eventBuffer->EventsStore.Events.size; i++) {
		TMEvent& ev = eventBuffer->EventsStore.Events.data[i + eventBuffer->EventsStore.Offset];
		
		int evTime = static_cast<int>(ev.Time);
		if (evTime >= 100010 + m_settingInputsMinTime && evTime <= upperBound) {
			if (utils::RandFloatRange(0, 1) < m_settingModifyChance) {
				unsigned char index = ev.GetNameIndex();
				if (m_settingMaxSteerDiff > 0 && (index == m_controlNames.steerLeftId || index == m_controlNames.steerRightId)) {
					minTime = (std::min)(minTime, input_mutation::TransformEventToAnalog(eventBuffer->EventsStore, m_controlNames, i));
					index = static_cast<unsigned char>(m_controlNames.steerId);
				}

				if (index == m_controlNames.steerId && m_settingMaxSteerDiff > 0) {
					int time = input_mutation::MutateSteerValue(ev, m_settingMaxSteerDiff);
					if (time == -1) {
						continue;
					}

					m_modified++;
					minTime = (std::min)(minTime, time);
				} else if (m_settingMaxTimeDiff > 0) {
					int time = input_mutation::MutateTimeValue(eventBuffer->EventsStore, m_controlNames, i, m_settingMaxTimeDiff, m_bestTime);
					if (time == -1) {
						continue;
					}

					m_modified++;
					minTime = (std::min)(minTime, time);
					eventBuffer->SortByTime();
				}
			}
		}
	}

	if (m_modified == 0) {
		return RandomNeighbour(simManager, nCall + 1);
	}

	return minTime;
}

bool BFSearchController::ProcessTriggers(SimulationManager& simManager, const int time, const Vec3& carPos, const Vec3& carVelocity, std::vector<TriggerHit>& hits)
{
	auto triggerManager = simManager.GetControllerByType<TriggerManager>();
	if (!triggerManager || triggerManager->Empty()) {
		return false;
	}

	Trigger3D* current = triggerManager->ProcessCurrentHit(carPos);
	if (current && current != m_currentTrigger) {
		float prevDist = current->Distance(m_prevPos);
		hits.emplace_back(time, prevDist, carVelocity.Length());
		fmt::print(fmt::fg(fmt::color::white), "Reached trigger: {}\n", hits.back());

		m_currentTrigger = current;
	}

	return true;
}

bool BFSearchController::AskClientForEvaluation(SimulationManager& simManager, const int time)
{
	auto& server = TM_IFACE->GetServer();
	if (server.HasRegisteredClient()) {
		BFEvaluationInfoData info = {
			m_phase,
			m_userTarget,
			time,
			m_modified,
			m_settingInputsMinTime,
			m_settingInputsMaxTime,
			m_settingMaxSteerDiff,
			m_settingMaxTimeDiff,
			m_settingOverrideStopTime,
			m_settingSearchForever,
			m_settingInputsSteerFill
		};

		auto response = server.CallOnBruteforceEvaluate(info);
		switch (response.decision)
		{
		case BFEvaluationDecision::kContinue:
			return false;
		case BFEvaluationDecision::kDoNothing:
			break;
		case BFEvaluationDecision::kAccept:
			if (m_phase != BFPhase::kSearch) {
				break;
			}

			PrintSaveInputs(simManager, -1);
			if (m_settingSearchForever) {
				ResetWithNewSolution(simManager);
				return true;
			} else {
				m_simulating = false;
			}

			StartNewIteration(simManager);
			break;
		case BFEvaluationDecision::kReject:
			if (m_phase != BFPhase::kSearch) {
				break;
			}

			StartNewIteration(simManager, response.rewindTime);
			break;
		case BFEvaluationDecision::kStop:
			m_simulating = false;
			break;
		default:
			break;
		}

		return true;
	}

	return false;
}

void BFSearchController::PrintSaveInputs(SimulationManager& simManager, const int score)
{
	m_improvements++;

	simManager.SaveCurrentInputs();
	const auto& dump = simManager.GetInputsDump();
	fmt::print(fmt::fg(kSuccessColor), "{}\n", dump);

	auto console = CONSOLE;
	auto scriptPath = console->GetValue<std::string>("scripts_folder");
	if (scriptPath.empty()) {
		scriptPath = utils::GetDataPath();
		utils::AppendPath(scriptPath, "Scripts");
	}

	auto filename = console->GetValue<std::string>("bf_result_filename");
	boost::remove_erase_if(filename, boost::is_any_of("\\/"));
	if (filename.empty()) {
		fmt::print(fmt::fg(kErrorColor), "Could not save solution. Variable bf_result_filename is empty.");
		return;
	}

	utils::AppendPath(scriptPath, filename);

	std::ofstream resultFile(scriptPath);
	if (!resultFile.good()) {
		fmt::print(fmt::fg(kErrorColor), "Could not save solution. Failed to open {}\n", scriptPath);
		return;
	} else {
		fmt::print(fmt::fg(kSuccessColor), "The solution has been saved to {}\n", scriptPath);
	}

	if (score != -1) {
		resultFile << "# Time: " << score << ", iterations: " << m_iterations << "\n";
	} else {
		resultFile << "# Iterations: " << m_iterations << "\n";
	}

	resultFile << dump;
	resultFile.close();
}
