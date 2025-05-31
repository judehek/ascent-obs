use serde::{Serialize, Deserialize};

/// Represents the different rate control modes available for video encoding.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum RateControlMode {
    /// Constant Bitrate - maintains a consistent bitrate throughout the video.
    /// Best for streaming where bandwidth is limited.
    #[serde(rename = "cbr")]
    Cbr,
    
    /// Variable Bitrate - adjusts the bitrate based on scene complexity.
    /// Provides better quality at the same average bitrate as CBR.
    #[serde(rename = "vbr")]
    Vbr,
    
    /// Constant Rate Factor - maintains consistent quality by varying bitrate.
    /// Lower values mean better quality (typically 18-28 is a good range).
    #[serde(rename = "crf")]
    Crf,
    
    /// Constant Quantizer Parameter - similar to CRF but uses a different scale.
    /// Lower values mean better quality (typically 16-23 is a good range).
    #[serde(rename = "cqp")]
    Cqp,
}

impl Default for RateControlMode {
    fn default() -> Self {
        Self::Cbr
    }
}

impl RateControlMode {
    /// Returns an array of all available rate control modes with their display labels.
    pub fn all() -> [RateControlOption; 4] {
        [
            RateControlOption {
                label: "Default (Constant Bitrate)",
                value: Self::Cbr,
            },
            RateControlOption {
                label: "Variable Bitrate (VBR)",
                value: Self::Vbr,
            },
            RateControlOption {
                label: "Constant Quality (CRF)",
                value: Self::Crf,
            },
            RateControlOption {
                label: "Constant Quantizer Parameter (CQP)",
                value: Self::Cqp,
            },
        ]
    }
    
    /// Converts the enum variant to its string representation.
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Cbr => "cbr",
            Self::Vbr => "vbr",
            Self::Crf => "crf",
            Self::Cqp => "cqp",
        }
    }
}

/// Represents a rate control mode option with a display label.
#[derive(Debug, Clone, Copy)]
pub struct RateControlOption {
    pub label: &'static str,
    pub value: RateControlMode,
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json;
    
    #[test]
    fn test_serialization() {
        assert_eq!(serde_json::to_string(&RateControlMode::Cbr).unwrap(), "\"cbr\"");
        assert_eq!(serde_json::to_string(&RateControlMode::Vbr).unwrap(), "\"vbr\"");
        assert_eq!(serde_json::to_string(&RateControlMode::Crf).unwrap(), "\"crf\"");
        assert_eq!(serde_json::to_string(&RateControlMode::Cqp).unwrap(), "\"cqp\"");
    }
    
    #[test]
    fn test_deserialization() {
        assert_eq!(serde_json::from_str::<RateControlMode>("\"cbr\"").unwrap(), RateControlMode::Cbr);
        assert_eq!(serde_json::from_str::<RateControlMode>("\"vbr\"").unwrap(), RateControlMode::Vbr);
        assert_eq!(serde_json::from_str::<RateControlMode>("\"crf\"").unwrap(), RateControlMode::Crf);
        assert_eq!(serde_json::from_str::<RateControlMode>("\"cqp\"").unwrap(), RateControlMode::Cqp);
    }
    
    #[test]
    fn test_all_options() {
        let options = RateControlMode::all();
        assert_eq!(options.len(), 4);
        assert_eq!(options[0].label, "Default (Constant Bitrate)");
        assert_eq!(options[0].value, RateControlMode::Cbr);
        assert_eq!(options[1].label, "Variable Bitrate (VBR)");
        assert_eq!(options[1].value, RateControlMode::Vbr);
    }
}
