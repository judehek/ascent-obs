#pragma once
#include <memory>
#include <util\util.hpp>
#include "..\utils\utils.h"

class OWOBSController {
public:
	OWOBSController();
	virtual ~OWOBSController();

	bool Init();
	static bool EncoderAvailable(const char *encoder);
	static bool SourceAvailable(const char *source_id);

	const char *InputAudioSource() const;
	const char *OutputAudioSource() const;
	const char *GetRenderModule() const;

	void CreateDebugWindow();

	// OBSBasic::ResetVideo()
	int ResetVideo();
	bool ResetAudio();

	config_t* Config() {
		config_t *config = config_;
		return config; 
	}

	void AddSource(obs_source_t *source, bool stretchToOutputSize = false);

	void StretchToOutputSize(obs_source_t *source);

	obs_scene_t * scene() { return (obs_scene_t *)scene_.get(); }

	obs_display_t *display() { return display_->get(); }

private:
	void LoadConfig();

	void CreateScene();

	// OBSBasic
	bool InitBasicConfigDefaults();

private:
	ConfigFile config_;

	std::unique_ptr<Utils::owobs::SceneContext> scene_;

	std::unique_ptr<Utils::owobs::DisplayContext> display_;
};

typedef std::shared_ptr<OWOBSController> OWOBSControllerPtr;
