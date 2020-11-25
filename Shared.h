#pragma once
#define MAPPED_FILE_NAME L"TMInterface"
#define BUF_SIZE 16384
#define MAX_SERVERS 16
#include <cstdio>
#include <array>
#include <string_view>
#include <ostream>
#include <vector>
#include "TrackMania.h"

enum class ContextMode {
	SIMULATION = 0,
	RUN = 1
};

/**
 * For server calls, the server takes care of clearing the buffer,
 * however the client has to respond with C_PROCESSED_CALL that contains the server
 * message type in the response.
 *
 * For client calls, the client takes care of clearing the buffer,
 * as the server does not care if it's response was processed or not.
 */
enum class MessageType : uint32_t
{
	/**
	  * S_RESPONSE: a response from the server. This can be a response to
	  * a client call such as C_SIM_GET_STATE or C_SET_KEY_STATES.
	  * Data: dynamic, depends on what was the call type.
	  */
	S_RESPONSE = 1,

	/**
	 * S_ON_RUN_STEP: server call to client to process a new run step.
	 * This will be only called when feature mode is RUN.
	 * Client must respond with C_PROCESSED_CALL and the message type of S_ON_RUN_STEP.
	 * Data: CallOnRunStepData
	 */
	S_ON_RUN_STEP,

	/**
	 * S_ON_SIM_BEGIN: server call to client that signifies a new simulation session.
	 * This will be only called when feature mode is SIMULATION.
	 * Client must respond with C_PROCESSED_CALL and the message type of S_ON_SIM_BEGIN.
	 * Data: EmptyData
	 */
	S_ON_SIM_BEGIN,
	/**
	 * S_ON_SIM_STEP: server call to client that signifies a new simulation step.
	 * This will be only called when feature mode is SIMULATION.
	 * Client must respond with C_PROCESSED_CALL and the message type of S_ON_SIM_STEP.
	 * Data: CallOnSimStepData
	 */
	S_ON_SIM_STEP,
	/**
	 * S_ON_SIM_AFTER_STEP: server call to client that signifies a the end of a simulation step.
	 * This call only applies to simulation context.
	 *
	 * This will be only called when feature mode is SIMULATION.
	 * Client must respond with C_PROCESSED_CALL and the message type of S_ON_SIM_AFTER_STEP.
	 * Data: CallOnRunStepData
	 */
	S_ON_SIM_AFTER_STEP,
	/**
	 * S_ON_CHECKPOINT_COUNT_CHANGED: server call to client that signifies that checkpoint state changed.
	 *
	 * Used for detecting when a checkpoint was passed.
	 * This is called immediately after the game sets a new checkpoint state.
	 * Finish also counts as a checkpoint. If you do not want to end the simulation
	 * to end after the finish, override the checkpoint state using C_SIM_REWIND_TO_STATE
	 * and set last checkpoint's time to int32 max value. This will make the game
	 * run the simulation for a little bit longer which is the time to rewind back
	 * to a different state.
	 *
     * This will be only called when feature mode is SIMULATION.
	 * Client must respond with C_PROCESSED_CALL and the message type of S_ON_CHECKPOINT_COUNT_CHANGED.
	 * Data:
	 */
	S_ON_CHECKPOINT_COUNT_CHANGED,
	/**
	 *
	 */
	S_ON_LAPS_COUNT_CHANGED,
	/**
	 * C_REGISTER: client call to server to register a new client.
	 * This will be only called when feature mode is SIMULATION.
	 *
	 * To connect to a server, a client must first send the C_REGISTER message.
	 * The server sends an empty S_RESPONSE message back.
	 * Data: CallOnCheckpointCountChangedData
	 */
	C_REGISTER,
	/**
	 * C_DEREGISTER: client call to server to deregister an existing client.
	 * This will be only called when feature mode is SIMULATION.
	 *
	 * To disconnect from a server, a client must send the C_DEREGISTER message.
	 * The server sends an empty S_RESPONSE message back.
	 * Data: EmptyData
	 */
	C_DEREGISTER,
	/**
	 * C_PROCESSED_CALL: client response to the server that it processed a server call.
	 * This message must be sent back as a response to messages:
	 *     * S_ON_RUN_STEP, 
	 *     * S_ON_SIM_BEGIN
	 *     * S_ON_SIM_STEP
	 *     * S_ON_SIM_AFTER_STEP
	 *     * S_ON_CHECKPOINT_COUNT_CHANGED
	 * The server does not send any response to this message,
	 * instead, the server buffer is cleared.
	 * Data: ProcessedCallData
	 */
	C_PROCESSED_CALL,
	/**
	 * C_SET_KEY_STATES: client call to server to set key states ingame.
	 * This call only applies to run context.
	 * 
	 * Sets individual key states for the car. If successfully applied,
	 * key states are guaranteed to be applied at next step of the run.
	 * If you want to apply a key state that happens at 500ms, call
	 * send this message at 490ms (one step before).
	 * Data: SetKeyStatesData
	 */
	C_SET_KEY_STATES,
	/**
	 * C_RESPAWN: client call to server to respawn ingame.
	 * This call only applies to run context.
	 *
	 * Respawns the car immediately and responds when the input was sent.
	 * Keep in mind that this action is not fully supported because
	 * the input is sent through Windows API. It is not guaranteed
	 * that the input will be injected in time for the current game time.
	 * Data: EmptyData
	 */
	C_RESPAWN,
	/**
	 * C_SIM_REWIND_TO_STATE: client call to server to rewind to a simulation state.
	 * 
	 * Rewinds to a particular simulation state sent by the client. 
	 * This function should be called only at simulation step and not after it.
	 * For more information on simulation states, see SimStateData.
	 * Data: SimStateData
	 */
	C_SIM_REWIND_TO_STATE,
	/**
	 * C_SIM_REWIND_TO_TIME: client call to server to rewind to a specific simulation time.
	 *
	 * TMInterface internally saves all states throughout the simulation automatically and calling this
	 * function will rewind to the state at that time.
	 * Data: SimRewindToTimeData
	 */
	C_SIM_REWIND_TO_TIME,
	/**
	 * C_SIM_GET_STATE: client call to server to retrieve current simulation state.
	 *
	 * Because returned data is dynamic in size (because of checkpoint information)
	 * this can return RESPONSE_TOO_LONG error code if the message is too long.
	 * The response data will still be sent, but it's contents will cut off at the
	 * end of the buffer.
	 *
	 * Responds with SimStateData. For more information
	 * on simulation states, see SimStateData.
	 * Data: EmptyData
	 */
	C_SIM_GET_STATE,
	/**
	 * C_SIM_GET_EVENT_BUFFER: client call to server to retrieve current simulation event buffer.
	 * 
	 * Because returned data is dynamic in size this can return RESPONSE_TOO_LONG 
	 * error code if the message is too long.
	 * The response data will still be sent, but it's contents will cut off at the
	 * end of the buffer. For more information, see #EventBuffer.
	 * Data: SimEventBufferData
	 */
	C_SIM_GET_EVENT_BUFFER,
	/**
	 * C_SIM_GET_CONTROL_NAMES: client call to server to retrieve current simulation event bindings.
	 *
	 * An event in an event buffer only contains a reference to what event should be executed. This 
	 * reference is a 1-byte index which is used to retrieve the actual event name.
	 * Each member in the returned structure signifies the index for this event.
	 * Data: ControlNamesData
	 */
	C_SIM_GET_CONTROL_NAMES,
	/**
	 * C_GET_CONTEXT_MODE: client call to server to get current context mode.
	 *
	 * Gets the current context mode, that is whether TMInterface is currently
	 * in a simulation or in a real run.
	 * Available modes: RUN, SIMULATION.
	 * Data: EmptyData
	 */
	C_GET_CONTEXT_MODE,
	/**
	 * C_SIM_SET_EVENT_BUFFER: client call to server to set a new event buffer.
	 *
	 * Sets a new event buffer for the simulation to use. The buffer will be used by
	 * the game in the next simulation step. The new event buffer has to have the same
	 * length as the one returned by C_SIM_GET_EVENT_BUFFER.
	 * You can dynamically stream events by calling this function every simulation step
	 * For more information, see #EventBuffer.
	 * Data: SimEventBufferData
	 */
	C_SIM_SET_EVENT_BUFFER,
	/**
	 * C_GET_CHECKPOINT_STATE: client call to server to retrieve current checkpoint state.
	 *
	 * Gets the checkpoint state associated with the current simulation/run.
	 * Because returned data is dynamic in size this can return RESPONSE_TOO_LONG 
	 * error code if the message is too long.
	 * Data: EmptyData
	 */
	C_GET_CHECKPOINT_STATE,
	/**
	 * C_SET_CHECKPOINT_STATE: client call to server to set a new checkpoint state.
	 *
	 * Sets a new checkpoint state for the current simulation/run. The new checkpoint
	 * state has to have the same length as the one returned by C_SIM_GET_CHECKPOINT_STATE.
	 * For more information, see #TMCheckpoint.
	 * Data: CheckpointData
	 */
	C_SET_CHECKPOINT_STATE,
	/**
	 * C_SET_GAME_SPEED: client call to server to set current game speed.
	 *
	 * Sets the game speed to the sent value. This affects the entirety of
	 * the game. Inputs set using C_SET_KEY_STATES will work correctly across
	 * most speeds, however C_RESPAWN may not work correctly depending on the speed applied.
	 * Data: SetGameSpeedData
	 */
	C_SET_GAME_SPEED,
	/**
	 * C_SET_EXECUTE_COMMANDS: client call to server to toggle executing commands.
	 *
	 * The server provides a built-in way to execute commands from a file.
	 * This call provides a way to disable/enable executing any commands
	 * by the server
	 * Data: SetExecuteCommandsData
	 */
	C_SET_EXECUTE_COMMANDS,
	/**
	 * C_SET_TIMEOUT: client call to server to set timeout value used by the server.
	 *
	 * If the client does not respond in the default timeout (2000ms/2s) the server
	 * ignores the response and continues execution as if no call happened.
	 * You can configure this timeout by calling this function with the provided timeout.
	 * Set timeout to -1 if you do not want the server to timeout ever.
	 * Data: SetTimeoutData
	 */
	C_SET_TIMEOUT,
	ANY
};

inline constexpr std::ostream& operator<<(std::ostream& os, const MessageType type)
{
#define ENUMSTR(name) case MessageType::##name: os << #name; break;
	switch (type)
	{
		ENUMSTR(S_RESPONSE)
		ENUMSTR(S_ON_RUN_STEP)
		ENUMSTR(S_ON_SIM_BEGIN)
		ENUMSTR(S_ON_SIM_STEP)
		ENUMSTR(S_ON_SIM_AFTER_STEP)
		ENUMSTR(S_ON_CHECKPOINT_COUNT_CHANGED)
		ENUMSTR(S_ON_LAPS_COUNT_CHANGED)
		ENUMSTR(C_REGISTER)
		ENUMSTR(C_DEREGISTER)
		ENUMSTR(C_PROCESSED_CALL)
		ENUMSTR(C_GET_CONTEXT_MODE)
		ENUMSTR(C_SET_KEY_STATES)
		ENUMSTR(C_SIM_REWIND_TO_STATE)
		ENUMSTR(C_SIM_REWIND_TO_TIME)
		ENUMSTR(C_SIM_GET_STATE)
		ENUMSTR(C_SIM_GET_EVENT_BUFFER)
		ENUMSTR(C_SIM_SET_EVENT_BUFFER)
		ENUMSTR(C_SET_GAME_SPEED)
		ENUMSTR(C_SET_EXECUTE_COMMANDS)
		ENUMSTR(C_SET_TIMEOUT)
		ENUMSTR(ANY)
	default:
		break;
	}

#undef ENUMSTR
	return os;
}

struct EmptyData
{
	int32_t reserved;
};

struct ProcessedCallData 
{
	MessageType which = MessageType::ANY;
};

struct GetContextModeData
{
	ContextMode mode = ContextMode::RUN;
};

struct SetKeyStatesData
{
	int32_t left = -1;
	int32_t right = -1;
	int32_t up = -1;
	int32_t down = -1;
	int32_t steer = MAXINT32;
};

struct SimRewindToTimeData
{
	uint32_t time = 0;
};

struct SetGameSpeedData
{
	double speed = 1.0;
};

struct SetExecuteCommandsData
{
	bool enable = true;
};

struct SetTimeoutData
{
	int32_t timeout = -1;
};

struct CallOnRunStepData
{
	int32_t time = 0;
};

struct CallOnSimStepData
{
	uint32_t time = 0;
};

struct CallOnCheckpointCountChangedData
{
	int current = 0;
	int target = 0;
};

struct CallOnLapsCountChangedData
{
	int current = 0;
};

enum SimStateFlags
{
	HAS_TIMERS = 0x1,
	HAS_STATE_1 = 0x2,
	HAS_STATE_2 = 0x4,
	HAS_STATE_3 = 0x8,
	HAS_STATE_4 = 0x10,
	HAS_CMD_BUFFER_CORE = 0x20,
	HAS_INPUT_STATE = 0x40
};

struct SimStateData
{
	uint32_t flags = 0;
	int32_t eventHistorySize = -1;
	int32_t currentHistoryIndex = -1;
	std::array<unsigned char, 212> timers{};
	std::array<unsigned char, 820> state1{};
	std::array<unsigned char, 2180> state2{};
	std::array<unsigned char, 3056> state3{};
	std::array<unsigned char, 72> state4{};
	std::array<unsigned char, 256> cmdBufferCore{};
	std::array<unsigned char, 120> inputState{};
	int32_t currentCpCount = 0;
	int32_t currentLapsCount = 0;
	// uint32_t cpStatesSize;
	// dynamic: uint32_t[cpStatesSize]
	// uint32_t cpTimesSize;
	// dynamic: TMCheckpoint[cpTimesSize]

	int getTime() const
	{
		return *(int*)(&timers[0x4]);
	}
};

struct ControlNamesData
{
	int32_t raceIsRunningId = -1;
	int32_t finishLineId = -1;
	int32_t accelerateId = -1;
	int32_t brakeId = -1;
	int32_t steerLeftId = -1;
	int32_t steerRightId = -1;
	int32_t steerId = -1;
	int32_t respawnId = -1;
};

struct CheckpointData
{
	int32_t currentCpCount = 0;
	int32_t currentLapsCount = 0;
	// uint32_t cpStatesSize;
	// dynamic: uint32_t[cpStatesSize]
	// uint32_t cpTimesSize;
	// dynamic: TMCheckpoint[cpTimesSize]
};

struct SimEventBufferData
{
	unsigned eventsDuration = 0;
	// uint32_t eventsSize;
	// dynamic: TMEvent[eventsSize]
};

enum ErrorCode
{
	NONE = 0,
	RESPONSE_TOO_LONG,

	CLIENT_ALREADY_REGISTERED,

	NO_EVENT_BUFFER,
	INVALID_EVENT_BUFFER_SIZE,

	NO_PLAYER_INFO
};

template<typename T>
struct Message
{
	MessageType type = MessageType::ANY;
	int32_t errorCode = 0;
	T data = {};
};
