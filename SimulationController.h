#pragma once
#include <memory>
#include "GameEventBuffer.h"

class SimulationController
{
public:
	SimulationController();
	virtual ~SimulationController();
	virtual void onRunStep(const int time)
	{

	}

	virtual void onSimulationBegin()
	{

	}

	virtual void onSimulationEnd()
	{

	}

	virtual bool onSimulationStep(const int time)
	{
		return true;
	}

	virtual void onAfterSimulationStep()
	{

	}

	virtual void onGameInputBufferAvailable(GameEventBuffer& inputBuffer)
	{

	}

	virtual void onRaceFinished() 
	{

	}

	virtual void onCheckpointCountChanged() 
	{

	}
};
