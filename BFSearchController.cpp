#include "BFSearchController.h"
#include <filesystem>
#include <fstream>
#define MODIFY_CHANCE 0.003
#define MAX_STEER_DIFF 20000
#define MAX_TIME_DIFF 50
#define EXTEND_STEER_INPUTS false

#define MIN_REQUIRED_TIME 0 // default
//#define MIN_REQUIRED_TIME 13000
#define MAX_REQUIRED_TIME INT_MAX - 100000 // default
//#define MAX_REQUIRED_TIME 13260 - 200

//#define MAX_NO_FINISH_STREAK 200  // default INT_MAX
#define MAX_NO_FINISH_STREAK INT_MAX

#define OVERRIDE_FINISH_TIME -1 // default
//#define OVERRIDE_FINISH_TIME 23810

//#define CHANGE_BASE_SOLUTION_CHANCE 0.008   // default 0
#define CHANGE_BASE_SOLUTION_CHANCE 0


BFSearchController::BFSearchController()
{
	m_customTriggers = {
		//{ {351, 88, 732}, {32, 8, 32} },
		//{ {776, 24, 668}, {32, 8, 32} },
		//{ {956, 23, 560}, {64, 8, 32} }, // trigger 1
		//{ {640 - 32, 23, 156}, {32, 8, 64} }, // trigger 2
		//{ {520 - 32, 72, 156}, {64, 90, 64} } // trigger 2
	};
}

void BFSearchController::onSimulationBegin()
{
	m_simulating = true;
	TM_IFACE.setSpeed(100);
	m_targetCpTimes.clear();
	m_currentCpTimes.clear();
	m_currentTriggerTimes.clear();
	m_targetTriggerTimes.clear();
	m_phase = INITIAL;
}

void BFSearchController::onSimulationEnd()
{
	m_simulating = false;
	TM_IFACE.setSpeed(1);
}

bool BFSearchController::onSimulationStep(const int time)
{
	if (!m_simulating) {
		return true;
	}

	auto& simManager = TM_IFACE.getSimulationManager();
	auto& eventBuffer = simManager.getGameEventBuffer();

	if (m_phase == INITIAL) {
		if (time == 2600) {
			auto state = simManager.saveState();
			if (state) {
				m_startState = std::move(*state);
			}

			//if (EXTEND_STEER_INPUTS) {
			//	extendSteerInputs();
			//}

			readInputsFromBuffer(m_originalInputs);
			/*auto buffer = eventBuffer.getBuffer();
			m_originalInputs.reserve(buffer->arr.size - buffer->offset);
			for (int i = 0; i < buffer->arr.size; i++) {
				m_originalInputs.push_back(buffer->arr.data[i + buffer->offset]);
			}*/

			m_currentInputs = m_originalInputs;
		}

		if (m_currentTrigger >= 0 && !m_customTriggers.empty()) {
			auto pos = simManager.getCarPosition();
			if (m_customTriggers[m_currentTrigger].distance(pos) == 0) {
				float prevDist = m_customTriggers[m_currentTrigger].distance(m_prevPos);
				m_targetTriggerTimes.push_back({ time - 2610, prevDist });
				std::cout << m_targetTriggerTimes.back() << "\n";
				if (++m_currentTrigger > m_customTriggers.size() - 1) {
					m_currentTrigger = -1;
				}
			}
		}

		m_prevPos = simManager.getCarPosition();
		return true;
	}

	bool jump = CHANGE_BASE_SOLUTION_CHANCE > 0 && Utils::randFloatRange(0, 1) <= CHANGE_BASE_SOLUTION_CHANCE;
	if (m_currentCpTimes.size() >= m_targetCpTimes.size()) {
		if (m_currentCpTimes.back() < m_bestTime) {
			std::cout << "Found new better record: " << m_currentCpTimes.back() << " iterations: " << m_iterations << "\n";
			printSaveInputs();
			Sleep(3000);

			m_simulating = false;

			/*m_currentInputs.clear();
			auto buffer = eventBuffer.getBuffer();
			m_currentInputs.reserve(buffer->arr.size - buffer->offset);
			for (int i = 0; i < buffer->arr.size; i++) {
				m_currentInputs.push_back(buffer->arr.data[i + buffer->offset]);
			}

			m_targetTime = m_bestTime = m_currentFinishTime;*/
		}

		if (jump) {
			std::cout << "Jumping to a different solution with same time.\n";
			readInputsFromBuffer(m_currentInputs);
		}

		m_noFinishStreak = 0;
		reset();
	}

	if (m_currentTrigger >= 0 && !m_customTriggers.empty()) {
		auto pos = simManager.getCarPosition();
		if (m_customTriggers[m_currentTrigger].distance(pos) == 0) {
			float prevDist = m_customTriggers[m_currentTrigger].distance(m_prevPos);
			m_currentTriggerTimes.push_back({ time - 2610, prevDist });
			std::cout << m_currentTriggerTimes.back() << "\n";
			if (++m_currentTrigger > m_customTriggers.size() - 1) {
				m_currentTrigger = -1;
			}
		}

		int lastIndex = m_currentTriggerTimes.size() - 1;
		if (lastIndex >= 0 && m_currentTriggerTimes[lastIndex] < m_targetTriggerTimes[lastIndex]) {
			std::cout << "Found a better trigger: " << m_currentTriggerTimes[lastIndex] << " < " << m_targetTriggerTimes[lastIndex] << "\n";
			printSaveInputs();
			Sleep(2000);

			m_simulating = false;
		}
	}


	if (simManager.getTime() - 2610 > m_targetCpTimes.back()) {
		m_noFinishStreak++;
		if (m_iterations % 1000 == 0) {
			std::cout << "-1 [iter: " << m_iterations << "]\n";
		} else {
			std::cout << "-1 (" << m_modified << ")\n";
		}

		if (m_noFinishStreak > MAX_NO_FINISH_STREAK) {
			std::cout << "Jumping to a the original solution - exceeded max no finish streak.\n";
			m_currentInputs = m_originalInputs;
			m_noFinishStreak = 0;
		} else if (jump) {
			std::cout << "Jumping to a different solution with no finish time.\n";
			readInputsFromBuffer(m_currentInputs);
		}

		reset();
	}

	m_prevPos = simManager.getCarPosition();
	return true;
}

void BFSearchController::reset()
{
	m_currentCpTimes.clear();
	m_currentTriggerTimes.clear();
	m_currentTrigger = m_customTriggers.empty() ? -1 : 0;
	auto& simManager = TM_IFACE.getSimulationManager();
	simManager.rewindToState(m_startState);
	randomNeighbour();
}

void BFSearchController::onCheckpointCountChanged()
{
	if (!m_simulating) {
		return;
	}

	auto& simManager = TM_IFACE.getSimulationManager();
	if (simManager.getFinished()) {
		std::cout << simManager.getTime() - 2610 << " (" << m_modified << ")" << "\n";
		if (m_phase == INITIAL) {
			if constexpr (OVERRIDE_FINISH_TIME != -1) {
				m_targetCpTimes.push_back(OVERRIDE_FINISH_TIME);
			} else {
				m_targetCpTimes.push_back(simManager.getTime() - 2610);
			}

			startSearchPhase();
		} else {
			m_currentCpTimes.push_back(simManager.getTime() - 2610);
		}

		preventSimulationEnd(simManager);
	} else if (m_phase == INITIAL) {
		m_targetCpTimes.push_back(simManager.getTime() - 2610);
	} else if (m_phase == SEARCH) {
		int t = simManager.getTime() - 2610;
		m_currentCpTimes.push_back(t);
		/*int lastIndex = m_currentCpTimes.size() - 1;
		if (t < m_targetCpTimes[lastIndex] && lastIndex == 1) {
			std::cout << "Found a better checkpoint: " << t << " < " << m_targetCpTimes[lastIndex] << "\n";
			printSaveInputs();
			Sleep(2000);
		}*/
	}
}

void BFSearchController::startSearchPhase()
{
	auto& simManager = TM_IFACE.getSimulationManager();
	auto names = simManager.getGameEventBuffer().getControlNames();
	m_steerIndex = std::distance(names.begin(), std::find(names.begin(), names.end(), "Steer (analog)"));
	std::cout << "Steer index: " << (int)m_steerIndex << "\n";

	if (EXTEND_STEER_INPUTS) {
		extendSteerInputs();
	}

	m_currentTrigger = m_customTriggers.empty() ? -1 : 0;

	//populateSteerSequences();

	m_phase = SEARCH;
	m_bestTime = m_targetCpTimes.back();
	randomNeighbour();
}

void BFSearchController::readInputsFromBuffer(std::vector<TMEvent>& inputs)
{
	auto& eventBuffer = TM_IFACE.getSimulationManager().getGameEventBuffer();
	auto buffer = eventBuffer.getBuffer();
	inputs.clear();
	inputs.reserve(buffer->arr.size - buffer->offset);
	for (int i = 0; i < buffer->arr.size; i++) {
		inputs.push_back(buffer->arr.data[i + buffer->offset]);
	}
}

void BFSearchController::preventSimulationEnd(SimulationManager& simManager)
{
	auto cps = simManager.getGamePlayerInfo().getCheckpointArray();
	if (cps->size > 0) {
		int last = cps->size - 1;
		cps->data[last].time = UINT32_MAX;
	}
}

void BFSearchController::randomNeighbour()
{
	m_modified = 0;
	auto& simManager = TM_IFACE.getSimulationManager();
	auto& eventBuffer = simManager.getGameEventBuffer();
	auto buffer = eventBuffer.getBuffer();
	for (int i = 0; i < buffer->arr.size; i++) {
		TMEvent& ev = buffer->arr.data[i + buffer->offset];
		ev.data = m_currentInputs[i].data;
		ev.time = m_currentInputs[i].time;
		if (ev.getNameIndex() == m_steerIndex && ev.time >= 100000 + MIN_REQUIRED_TIME && ev.time <= 100000 + MAX_REQUIRED_TIME) {
			if (Utils::randFloatRange(0, 1) < MODIFY_CHANCE) {
				//if (Utils::randFloatRange(0.0001, 1) < 0.5) {
				//	int diff = Utils::randRange(-MAX_TIME_DIFF, MAX_TIME_DIFF);
				//	ev.time += diff;
				//} else {
				auto steer = ev.getEnabled();
				short diff = Utils::randRange(-MAX_STEER_DIFF, MAX_STEER_DIFF);
				int axis = GameEventBuffer::eventToSteerValue(&ev);
				axis += diff;
				if (axis > 65535 || axis < -65536) {
					continue;
				}

				GameEventBuffer::writeSteerValueToEvent(&ev, axis);
				//}

				m_modified++;
				//((SteerData*)&ev.data)->steer = steer + diff;
			}
		}
	}

	if (m_modified == 0) {
		randomNeighbour();
		return;
	}

	m_iterations++;
}

void BFSearchController::printSaveInputs()
{
	auto& eventBuffer = TM_IFACE.getSimulationManager().getGameEventBuffer();
	eventBuffer.saveCurrentInputs();
	std::cout << eventBuffer.getInputsDump() << "\n";

	//auto path = Utils::getDataPath();
	//if (path.empty()) {
	//	return;
	//}

	//auto searchResultsPath = path + "\\SearchResults";
	//if (!std::filesystem::exists(searchResultsPath) && !std::filesystem::create_directories(searchResultsPath)) {
	//	return;
	//}

	//time_t now = time(0);
	//struct tm* tstruct = localtime(&now);
	//char buf[80];
	//strftime(buf, sizeof(buf), "%Y_%m_%d_%X", tstruct);

	//std::ofstream file(searchResultsPath + "\\config.txt", std::ios_base::app);
}

void BFSearchController::populateSteerSequences()
{
	m_steerSequences.clear();
	std::cout << "Sequences:\n";
	auto& simManager = TM_IFACE.getSimulationManager();
	auto& eventBuffer = simManager.getGameEventBuffer();
	auto buffer = eventBuffer.getBuffer();
	std::vector<unsigned> seq;
	for (int i = buffer->arr.size - 1; i >= 0; i--) {
		TMEvent& ev = buffer->arr.data[i + buffer->offset];
		if (ev.getNameIndex() == m_steerIndex) {
			seq.push_back(i);
			int axis = GameEventBuffer::eventToSteerValue(&ev);
			std::cout << axis << " ";
			if (axis >= -10 && axis <= 10) {
				m_steerSequences.push_back(seq);
				seq.clear();
				std::cout << "\n";
			}
		}
	}
}

void BFSearchController::extendSteerInputs()
{
	m_originalInputs.clear();

	auto& eventBuffer = TM_IFACE.getSimulationManager().getGameEventBuffer();
	TMEventBuffer* buffer = eventBuffer.getBuffer();

	auto names = TM_IFACE.getSimulationManager().getGameEventBuffer().getControlNames();
	int leftIndex = std::distance(names.begin(), std::find(names.begin(), names.end(), "Steer left"));
	int rightIndex = std::distance(names.begin(), std::find(names.begin(), names.end(), "Steer right"));

	TMEvent* lastSteerEvent = nullptr;
	m_originalInputs.resize(buffer->offset);
	for (int i = 0; i <= buffer->eventsDuration; i += 10) {
		bool hasSteer = false;
		for (unsigned j = 0; j < buffer->arr.size; j++) {
			TMEvent& ev = buffer->arr.data[j + buffer->offset];
			if (ev.time != i + 100000) {
				continue;
			}

			byte index = ev.getNameIndex();
			if (index == m_steerIndex) {
				hasSteer = true;
				lastSteerEvent = &ev;
			} else if (index == leftIndex || index == rightIndex) {
				if (ev.getEnabled()) {
					hasSteer = true;
				}

				lastSteerEvent = nullptr;
			}

			m_originalInputs.insert(m_originalInputs.begin() + buffer->offset, ev);
		}

		if (!hasSteer && lastSteerEvent && i > 0) {
			TMEvent ev;
			ev.time = i + 100000;
			ev.data = lastSteerEvent->data;
			m_originalInputs.insert(m_originalInputs.begin() + buffer->offset, ev);
		}
	}

	// From now on we have to ensure that data() in m_originalInputs returns a valid pointer
	buffer->arr.data = m_originalInputs.data();
	buffer->arr.size = m_originalInputs.size() - buffer->offset;
	buffer->arr.capacity = m_originalInputs.size();
	std::cout << "Start: " << HEX(buffer->arr.data + buffer->offset) << ", end: " << HEX(buffer->arr.data + buffer->offset + buffer->arr.size) << "\n";

	m_currentInputs = std::vector(m_originalInputs.begin() + buffer->offset, m_originalInputs.end());
	std::cout << "Extended inputs: True\n";
}


//void randomNeighbour()
//{
//	auto& simManager = TM_IFACE.getSimulationManager();
//	auto& eventBuffer = simManager.getGameEventBuffer();
//	auto buffer = eventBuffer.getBuffer();

//	for (int i = 0; i < buffer->arr.size; i++) {
//		TMEvent& ev = buffer->arr.data[i + buffer->offset];
//		ev.data = m_currentInputs[i].data;
//		ev.time = m_currentInputs[i].time;
//	}

//	int modifiedSequences = Utils::randRange(1, (std::min)(3, (int)m_steerSequences.size()));
//	std::vector<int> indexes(m_steerSequences.size());
//	for (int i = 0; i < indexes.size(); i++) {
//		indexes[i] = i;
//	}

//	std::shuffle(indexes.begin(), indexes.end(), rng);

//	for (int i = 0; i < modifiedSequences; i++) {
//		int sequenceIndex = indexes[i];
//		for (auto index : m_steerSequences[sequenceIndex]) {
//			//float a = Utils::randFloatRange(1 - 0.04f, 1 + 0.04f);
//			int b = Utils::randRange(-50, 50);
//			TMEvent& ev = buffer->arr.data[index + buffer->offset];
//			int axis = GameEventBuffer::eventToSteerValue(&ev);
//			axis = axis + b;
//			if (axis > 65535 || axis < -65536) {
//				continue;
//			}
//			GameEventBuffer::writeSteerValueToEvent(&ev, axis);
//		}
//	}

//	m_iterations++;
//}