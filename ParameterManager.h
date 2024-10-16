#pragma once
#include "Osc.h"
#include <format>

enum DroneStage
{
	Idle,
	Sending,
	Sent,
	Finished,
};

class ParameterManager
{
public:
	void Start();
	void MoveDrone(int axis, float value);
	bool IsAvailable();

	template <typename T>
	void SetParameter(const char* parameter, T value);

	void HandleMsg(OSCPP::Server::Message const&);

private:
	template <typename T>
	bool CheckDuplicate(const char* parameter, T value);

private:
	std::atomic<DroneStage> m_droneStage = Idle;
	std::unique_ptr<Osc> m_osc{ nullptr };
	std::map<std::string, std::string> m_parameters;
};

template<typename T>
inline void ParameterManager::SetParameter(const char* parameter, T value)
{
	if (CheckDuplicate(parameter, value)) return;
	m_osc->SetParameter(std::format("danceCam/{}", parameter).c_str(), value);
}

template<typename T>
inline bool ParameterManager::CheckDuplicate(const char* parameter, T value)
{
	auto it = m_parameters.find(parameter);
	return it != m_parameters.end() && it->second == std::to_string(value);
}
