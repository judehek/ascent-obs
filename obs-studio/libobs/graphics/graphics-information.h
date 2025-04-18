
#pragma once
#include <string>
#include <vector>

/*
 * This is an API-independent graphics subsystem wrapper.
 *
 *   This allows the use of OpenGL and different Direct3D versions through
 * one shared interface.
 */

#ifdef __cplusplus
extern "C" {
#endif


// ascent-obs
struct adapter_info {
	int index;
	std::string name;
	bool hags_enabled;
	bool hags_enabled_by_default;
	std::string driver;
};

struct monitor_info {
	int adpater;
	int index;
	int left;
	int top;
	int cx;
	int cy;
	bool attachedToDesktop;
	float dpi;
	unsigned int refresh;
	std::string id;
	std::string alt_id;
	std::string friendly_name;
};

struct graphics_information {
	std::vector<adapter_info> adapters;
	std::vector<monitor_info> monitors;
};
#ifdef __cplusplus
}
#endif
