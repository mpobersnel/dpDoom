/*
**  Handling drawing a sprite
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#pragma once

#include "hardpolyrenderer.h"

class HardpolyRenderer;
class AActor;

class HardpolyRenderSprite : public HardpolyTranslucentObject
{
public:
	HardpolyRenderSprite(AActor *thing, subsector_t *sub, uint32_t subsectorDepth, double dist, float t1, float t2);

	void Setup(HardpolyRenderer *renderer) override;
	void Render(HardpolyRenderer *renderer) override;

	static bool GetLine(AActor *thing, DVector2 &left, DVector2 &right);
	static bool IsThingCulled(AActor *thing);
	static FTexture *GetSpriteTexture(AActor *thing, /*out*/ bool &flipX);

private:
	AActor *thing = nullptr;
	subsector_t *sub = nullptr;
	float SpriteLeft = 0.0f, SpriteRight = 1.0f;

	int startVertex = 0;
	FTexture *tex = nullptr;
};
