#include "Bridge.h"
#include "DevBenchTool.h"
#include "Settings.h"
#include "UI/Theme.h"
#include "UI/UI.h"
#include "utils/AddressLibraryGuard.h"

namespace
{
	void InitializeLogging()
	{
		auto path = logger::log_directory();
		if (!path) {
			SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory.");
		}
		*path /= "RaceMenuAtelier.log";

		auto log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		switch (a_message->type) {
		case SKSE::MessagingInterface::kPostPostLoad:
			RMA::Settings::Get().Load();
			RMA::UI::LoadTheme();
			if (!RMA::UI::Register()) {
				logger::critical("SKSE Menu Framework is not installed; RaceMenu Atelier stays inactive and RaceMenu keeps its own interface");
			}
			break;
		case SKSE::MessagingInterface::kPostLoad:
			RMA::DevBenchTool::Init(false);
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			if (RMA::UI::IsRegistered()) {
				RMA::Bridge::Install();
			}
			RMA::DevBenchTool::Init(true);
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLogging();

	// Address Library pre-check (the guard every mod of ours carries), BEFORE SKSE::Init, which opens the Address
	// Library itself: a missing file gets a message naming it, not CommonLib's bare failure, and the plugin loads inert
	if (!AddressLibraryGuard::Guard("RaceMenu Atelier")) {
		return true;
	}

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	logger::info("{} {} is loading", plugin->GetName(), plugin->GetVersion().string());

	SKSE::Init(a_skse);

	const auto messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		logger::critical("could not register the SKSE message listener");
		return false;
	}
	return true;
}
