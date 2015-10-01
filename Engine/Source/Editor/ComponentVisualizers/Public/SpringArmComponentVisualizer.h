// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"

class COMPONENTVISUALIZERS_API FSpringArmComponentVisualizer : public FComponentVisualizer
{
public:
	//~ Begin FComponentVisualizer Interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	//~ End FComponentVisualizer Interface
};