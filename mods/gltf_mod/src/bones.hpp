#pragma once

#include "types.h"

/**
 * Constants related to bone mapping.
 */
namespace slugcat::gltf::bones {

/**
 * VRM humanoid bone names.
 */
namespace vrm {

/**
 * Not a real VRM bone; used for remapping purposes.
 */
constexpr auto kBoneModRoot = "_root_";

// clang-format off

// Torso
constexpr auto kBoneHips       = "hips";
constexpr auto kBoneSpine      = "spine";
constexpr auto kBoneChest      = "chest";
constexpr auto kBoneUpperChest = "upperChest";
constexpr auto kBoneNeck       = "neck";

// Head
constexpr auto kBoneHead     = "head";
constexpr auto kBoneLeftEye  = "leftEye";
constexpr auto kBoneRightEye = "rightEye";
constexpr auto kBoneJaw      = "jaw";

// Leg
constexpr auto kBoneLeftUpperLeg  = "leftUpperLeg";
constexpr auto kBoneLeftLowerLeg  = "leftLowerLeg";
constexpr auto kBoneLeftFoot      = "leftFoot";
constexpr auto kBoneLeftToes      = "leftToes";
constexpr auto kBoneRightUpperLeg = "rightUpperLeg";
constexpr auto kBoneRightLowerLeg = "rightLowerLeg";
constexpr auto kBoneRightFoot     = "rightFoot";
constexpr auto kBoneRightToes     = "rightToes";

// Arm
constexpr auto kBoneLeftShoulder  = "leftShoulder";
constexpr auto kBoneLeftUpperArm  = "leftUpperArm";
constexpr auto kBoneLeftLowerArm  = "leftLowerArm";
constexpr auto kBoneLeftHand      = "leftHand";
constexpr auto kBoneRightShoulder = "rightShoulder";
constexpr auto kBoneRightUpperArm = "rightUpperArm";
constexpr auto kBoneRightLowerArm = "rightLowerArm";
constexpr auto kBoneRightHand     = "rightHand";

// I can't be arsed to fill out the fingers for now.

// clang-format on

}  // namespace vrm

/**
 * Link model joint IDs
 */
namespace link {

// clang-format off

// Names taken from al.bmd in Kmdl.arc
constexpr u16 kJointCenter    = 0x00;
constexpr u16 kJointBackbone1 = 0x01;
constexpr u16 kJointBackbone2 = 0x02;
constexpr u16 kJointNeck      = 0x03;
constexpr u16 kJointHead      = 0x04;
constexpr u16 kJointPod       = 0x05;
constexpr u16 kJointShoulderL = 0x06;
constexpr u16 kJointArmL1     = 0x07;
constexpr u16 kJointArmL2     = 0x08;
constexpr u16 kJointHandL     = 0x09;
constexpr u16 kJointWeaponL   = 0x0A;
constexpr u16 kJointShoulderR = 0x0B;
constexpr u16 kJointArmR1     = 0x0C;
constexpr u16 kJointArmR2     = 0x0D;
constexpr u16 kJointHandR     = 0x0E;
constexpr u16 kJointWeaponR   = 0x0F;
constexpr u16 kJointWaist     = 0x10;
constexpr u16 kJointClotchL   = 0x11; // [sic]
constexpr u16 kJointLegL1     = 0x12;
constexpr u16 kJointLegL2     = 0x13;
constexpr u16 kJointFootL     = 0x14;
constexpr u16 kJointToeL      = 0x15;
constexpr u16 kJointClotchR   = 0x16; // [sic]
constexpr u16 kJointLegR1     = 0x17;
constexpr u16 kJointLegR2     = 0x18;
constexpr u16 kJointFootR     = 0x19;
constexpr u16 kJointToeR      = 0x1A;
constexpr u16 kJointFSkirtL1  = 0x1B;
constexpr u16 kJointFSkirtL2  = 0x1C;
constexpr u16 kJointFSkirtR1  = 0x1D;
constexpr u16 kJointFSkirtR2  = 0x1E;
constexpr u16 kJointRSkirtL1  = 0x1F;
constexpr u16 kJointRSkirtL2  = 0x20;
constexpr u16 kJointRSkirtR1  = 0x21;
constexpr u16 kJointRSkirtR2  = 0x22;

// clang-format on

}

// clang-format off
inline std::pair<std::string, u16> const vrmBonesToLinkJoints[]{
    {vrm::kBoneModRoot,       link::kJointCenter},
    {vrm::kBoneSpine,         link::kJointBackbone1},
    {vrm::kBoneChest,         link::kJointBackbone2},
    {vrm::kBoneNeck,          link::kJointNeck},
    {vrm::kBoneHead,          link::kJointHead},
    {vrm::kBoneLeftShoulder,  link::kJointShoulderL},
    {vrm::kBoneLeftUpperArm,  link::kJointArmL1},
    {vrm::kBoneLeftLowerArm,  link::kJointArmL2},
    {vrm::kBoneLeftHand,      link::kJointHandL},
    {vrm::kBoneRightShoulder, link::kJointShoulderR},
    {vrm::kBoneRightUpperArm, link::kJointArmR1},
    {vrm::kBoneRightLowerArm, link::kJointArmR2},
    {vrm::kBoneRightHand,     link::kJointHandR},
    {vrm::kBoneHips,          link::kJointWaist},
    {vrm::kBoneLeftUpperLeg,  link::kJointLegL1},
    {vrm::kBoneLeftLowerLeg,  link::kJointLegL2},
    {vrm::kBoneLeftFoot,      link::kJointFootL},
    {vrm::kBoneLeftToes,      link::kJointToeL},
    {vrm::kBoneRightUpperLeg, link::kJointLegR1},
    {vrm::kBoneRightLowerLeg, link::kJointLegR2},
    {vrm::kBoneRightFoot,     link::kJointFootR},
    {vrm::kBoneRightToes,     link::kJointToeR},
};
// clang-format on

}  // namespace slugcat::gltf::bones