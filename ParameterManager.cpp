#include "framework.h"
#include "ParameterManager.h"

std::map<std::string, int> ignoreCheckList = {
    {"UpdateAxis", 0},
    {"UpdateDelta", 0},
    {"UpdateDrone", 0},
};

void ParameterManager::Start()
{
    m_osc = std::make_unique<Osc>(9001, 9000, [this](OSCPP::Server::Message const& m) {
        HandleMsg(m);
        });
    m_osc->Start();
}

void ParameterManager::MoveDrone(int axis, float value)
{
    if (axis < 3) {
        value /= 1000;
    }
    else {
        value /= 180;
    }
    SetParameter("UpdateAxis", axis);
    SetParameter("UpdateDelta", value);
    SetParameter("UpdateDrone", true);
    m_droneStage = Sending;

    while (m_droneStage != Finished) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool ParameterManager::IsAvailable()
{
	return m_droneStage == Finished || m_droneStage == Idle;
}

void ParameterManager::HandleMsg(OSCPP::Server::Message const& msg)
{
    // Get argument stream
    OSCPP::Server::ArgStream args(msg.args());

    if (std::string(msg.address()).find("/avatar/parameters/danceCam/") != 0) return;

	std::string parameter = msg.address() + 28;

    if (parameter == "UpdateDrone") {
        const bool value = args.boolean();
        if (value) {
            m_droneStage = Sent;
            OutputDebugString(L"Drone sent\n");
        }
        else if (m_droneStage == Sent) {
            m_droneStage = Finished;
            OutputDebugString(L"Drone finished\n");
        }
    }
    if (ignoreCheckList.find(parameter) == ignoreCheckList.end()) {
		const char tag = args.tag();
		if (tag == 'f') {
			const float value = args.float32();
			m_parameters[parameter] = std::to_string(value);
		}
		else if (tag == 'i') {
			const int value = args.int32();
			m_parameters[parameter] = std::to_string(value);
		}
        else {
			const bool value = args.boolean();
			m_parameters[parameter] = std::to_string(value);
        }
    }
}

