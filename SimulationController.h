/* Copyright Adam Bieñkowski <donadigos159@gmail.com> - All Rights Reserved
 * Unauthorized redistribution and copying of this software and its source code via any medium is strictly prohibited.
 * Proprietary and confidential.
 */

#pragma once
#include <memory>
#include "MwFoundations.h"
#include "SimulationManager.h"

class SimulationController
{
public:
	SimulationController();
	virtual ~SimulationController();
	virtual void OnRunStep(SimulationManager& simManager, const int time)
	{

	}

	virtual void OnSimulationBegin(SimulationManager& simManager)
	{

	}

	virtual void OnSimulationEnd(SimulationManager& simManager, const uint32_t result)
	{

	}

	virtual void OnSimulationStep(SimulationManager& simManager, const int time)
	{

	}

	virtual void OnInputRace(SimulationManager& simManager)
	{

	}

	virtual void OnRaceFinished(SimulationManager& simManager)
	{

	}

	virtual void OnCheckpointCountChanged(SimulationManager& simManager)
	{

	}
};
