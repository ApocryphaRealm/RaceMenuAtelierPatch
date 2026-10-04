#pragma once

namespace RMA::DevBenchTool
{
	// registers atelier.control with DevBench; retried on later SKSE messages until the last attempt
	void Init(bool a_lastAttempt);
}
