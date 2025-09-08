
#include "kernels.h"
#include "KernelRString.h"


std::string createKernelSource()
{
std::string strPreProc = "\
#include \"common_def.h\"\
";


//https://streamhpc.com/blog/2016-02-09/atomic-operations-for-floats-in-opencl-improved/
std::string strAtomicAddFloat = KERNEL_R_STRING(
void atomicAddFloat(volatile global float* addr, float val)
{
	union
	{
		unsigned int u32;
		float f32;
	} next, expected, current;

	current.f32 = *addr;

	do
	{
		expected.f32 = current.f32;
		next.f32 = expected.f32 + val;
		current.u32 = atomic_cmpxchg((volatile global unsigned int*)addr, expected.u32, next.u32);
	} while (current.u32 != expected.u32);
}
);

std::string strMat4MulVec4 = KERNEL_R_STRING(
float4 mat4MulVec4(float16 mat, float4 vec)
{
	return (float4)(
		mat.s0 * vec.x + mat.s1 * vec.y + mat.s2 * vec.z + mat.s3 * vec.w,
		mat.s4 * vec.x + mat.s5 * vec.y + mat.s6 * vec.z + mat.s7 * vec.w,
		mat.s8 * vec.x + mat.s9 * vec.y + mat.sA * vec.z + mat.sB * vec.w,
		mat.sC * vec.x + mat.sD * vec.y + mat.sE * vec.z + mat.sF * vec.w
	);
}
);

https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
std::string strRNG = KERNEL_R_STRING(
float RNG(uint* seed)
{
	uint state = *seed;
	*seed = *seed * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return ((word >> 22u) ^ word) / (float)UINT_MAX;
}
);

std::string strSierpinskiTriangle = KERNEL_R_STRING(
float2 sierpinskiTriangle(float2 p, uint* seed, uint iterations)
{
	for (uint i = 0; i < iterations; i++)
	{
		float2 target;
		float r = RNG(seed);
		if (r < 0.33f)
		{
			target = (float2)(0, 0);
		}
		else if (r < 0.66f)
		{
			target = (float2)(0.5, sqrt(0.75)) * 10.0f;
		}
		else
		{
			target = (float2)(1, 0) * 10.0f;
		}

		p = 0.5f * (p + target);
	}

	return p;
}
);

std::string strMengerSponge = KERNEL_R_STRING(
float2 mengerSponge(float2 p, uint* seed, uint iterations)
{
	const float one_third = 1.0f / 3.0f;
	const float two_third = 2.0f * one_third;
	for (uint i = 0; i < iterations; i++)
	{
		float2 target;
		float r = RNG(seed);
		if		(r < 0.125f) target = (float2)(0.000f, 0.0f);
		else if (r < 0.25f)	 target = (float2)(one_third, 0.0f);
		else if (r < 0.375f) target = (float2)(two_third, 0.0f);
		else if (r < 0.5f)   target = (float2)(0.000f, one_third);
		else if (r < 0.625f) target = (float2)(two_third, one_third);
		else if (r < 0.75f)  target = (float2)(0.000f, two_third);
		else if (r < 0.875f) target = (float2)(one_third, two_third);
		else				 target = (float2)(two_third, two_third);

		p = one_third * p + target;
	}

	return p;
}
);

//https://flam3.com/flame_draves.pdf
std::string strVariations = KERNEL_R_STRING(
void v1(float2* p)
{
	p->x = sin(p->x);
	p->y = sin(p->y);
}

void v2(float2* p)
{
	float r = length(*p);
	float r2Inv = 1.0f / (r * r);
	*p *= r2Inv;
}

void v3(float2* p)
{
	float r2 = dot(*p, *p);
	*p = (float2)(p->x * sin(r2) - p->y * cos(r2), p->x * cos(r2) + p->y * sin(r2));
}

void v4(float2* p)
{
	float r = length(*p);
	*p = 1.0f / r * (float2)((p->x - p->y) * (p->x + p->y), 2.0f * p->x * p->y);
}

void v5(float2* p)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	p->x = theta / PI;
	p->y = r - 1.0f;
}

void v6(float2* p)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	p->x = r * sin(theta + r);
	p->y = r * cos(theta - r);
}

void v7(float2* p)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	p->x = r * sin(theta * r);
	p->y = r * -cos(theta * r);
}

void v8(float2* p)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	p->x = sin(PI * r);
	p->y = cos(PI * r);
	*p *= theta / PI;
}

void v9(float2* p)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	p->x = cos(theta) + sin(r);
	p->y = sin(theta) - cos(r);
	*p *= 1.0f / r;
}

void v10(float2* p)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	p->x = sin(theta) / r;
	p->y = r * cos(theta);
}

void v11(float2* p)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	p->x = sin(theta) * cos(r);
	p->y = cos(theta) * sin(r);
}

void v12(float2* p)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	float p0 = sin(theta + r);
	float p1 = cos(theta - r);
	p0 *= p0 * p0;
	p1 *= p1 * p1;
	p->x = p0 + p1;
	p->y = p0 - p1;
	*p *= r;
}

void v13(float2* p, uint* seed)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	float omega = RNG(seed) < 0.5f ? 0.0f : PI;
	p->x = cos(theta * 0.5f + omega);
	p->y = sin(theta * 0.5f + omega);
	*p *= sqrt(r);
}

void v14(float2* p)
{
	if (p->x < 0.0f) p->x *= 2.0f;
	if (p->y < 0.0f) p->y *= 0.5f;
}

void v18(float2* p)
{
	*p = exp(p->x - 1.0f) * (float2)(cos(p->y * PI), sin(p->y * PI));
}

void v19(float2* p)
{
	float r = length(*p);
	float theta = atan2(p->x, p->y);
	p->x = cos(theta);
	p->y = sin(theta);
	*p *= pow(r, sin(theta));
}

void v20(float2* p)
{
	*p = (float2)(cos(PI * p->x) * cosh(p->y), -sin(PI * p->x) * sinh(p->y));
}

void v28(float2* p)
{
	float r2 = dot(*p, *p);
	*p *= 4.0f / (r2 + 4);
}

void v29(float2* p)
{
	p->x = sin(p->x);
}

void v42(float2* p)
{
	p->x = sin(p->x) / cos(p->y);
	p->y = tan(p->y);
}

void v48(float2* p)
{
	float a = sqrt(1.0f / ((p->x * p->x - p->y * p->y) * (p->x * p->x - p->y * p->y)));
	*p *= a;
}
);

std::string strF = KERNEL_R_STRING(
void F(float2* p, float3* c, local uint* variations, local float* colours, local float* weightThresholds,
	float weightTotal, uint numVariations, uint* seed)
{
	//pick a weighted-random variation to apply

	float weightedRandom = RNG(seed) * weightTotal;
	uint r = 0;
	while (r < numVariations)
	{
		if (weightedRandom < weightThresholds[r]) break;
		r++;
	}

	*c = 0.5f * (*c + (float3)(colours[r * 3 + 0], colours[r * 3 + 1], colours[r * 3 + 2]));

	uint v = variations[r];
	if (v == 0) return;
	else if (v == 1) v1(p);
	else if (v == 2) v2(p);
	else if (v == 3) v3(p);
	else if (v == 4) v4(p);
	else if (v == 5) v5(p);
	else if (v == 6) v6(p);
	else if (v == 7) v7(p);
	else if (v == 8) v8(p);
	else if (v == 9) v9(p);
	else if (v == 10) v10(p);
	else if (v == 11) v11(p);
	else if (v == 12) v12(p);
	else if (v == 13) v13(p, seed);
	else if (v == 14) v14(p);
	else if (v == 18) v18(p);
	else if (v == 19) v19(p);
	else if (v == 20) v20(p);
	else if (v == 28) v28(p);
	else if (v == 29) v29(p);
	else if (v == 42) v42(p);
	else if (v == 48) v48(p);
}
);

std::string strPlot = KERNEL_R_STRING(
	void plot(global float* renderTexture, float2 p, float3 c, float16 matView, uint texWidth, uint texHeight)
{
	//draw the sample point to the buffer

	//transform sample point to camera view
	float4 pClip = mat4MulVec4(matView, (float4)(p.x, p.y, 0.0f, 1.0f));

	float u = pClip.x * 0.5f + 0.5f;
	float v = pClip.y * 0.5f + 0.5f;

	//discard positions outside of the buffer
	int pixelX = u * texWidth;
	int pixelY = v * texHeight;
	if (pixelX < 0 || pixelX >= texWidth || pixelY < 0 || pixelY >= texHeight) return;

	//draw to buffer by accumulating pixel values
	uint pixelIndex = pixelY * texWidth + pixelX;
	atomicAddFloat(&renderTexture[pixelIndex * 4 + 0], c.x);
	atomicAddFloat(&renderTexture[pixelIndex * 4 + 1], c.y);
	atomicAddFloat(&renderTexture[pixelIndex * 4 + 2], c.z);
	atomicAddFloat(&renderTexture[pixelIndex * 4 + 3], 1.0f);
}
);

std::string strProduceSamples = KERNEL_R_STRING(
kernel void produceSamples(global float* renderTexture, global uint* variations, global float* colours, global float* weights,
	uint numVariations, uint initialIterations, uint iterations, float16 matView, uint texWidth, uint texHeight, uint frameNum,
	uint numSamples, local uint* lc_variations, local float* lc_colours, local float* lc_weightThresholds)
{
	//each thread describes one sample point which gets iterated on and drawn to renderTexture

	const uint i = get_global_id(0);
	if (i >= numSamples) return;

	//buffers used frequently, so copy to local memory to reduce global reads
	float weightTotal = 0;
	if (get_local_id(0) == 0)
	{
		for (uint j = 0; j < numVariations; j++)
		{
			lc_variations[j] = variations[j];

			lc_colours[j * 3 + 0] = colours[j * 3 + 0];
			lc_colours[j * 3 + 1] = colours[j * 3 + 1];
			lc_colours[j * 3 + 2] = colours[j * 3 + 2];
			
			weightTotal += weights[j];
			lc_weightThresholds[j] = weightTotal;
		}
	}

	weightTotal = lc_weightThresholds[numVariations - 1];

	barrier(CLK_GLOBAL_MEM_FENCE | CLK_LOCAL_MEM_FENCE);

	uint seed = i + frameNum * numSamples;
	RNG(&seed); //randomise the seed once before using

	float2 p = (float2)(RNG(&seed) * 2.0f - 1.0f, RNG(&seed) * 2.0f - 1.0f);
	float3 c = (float3)(RNG(&seed), RNG(&seed), RNG(&seed));

	//do some initial iterations to move away from unifom distribution in unit square
	for (uint j = 0; j < initialIterations; j++)
	{
		F(&p, &c, lc_variations, lc_colours, lc_weightThresholds, weightTotal, numVariations, &seed);
	}
	
	for (uint j = 0; j < iterations; j++)
	{
		//pick a random function
		F(&p, &c, lc_variations, lc_colours, lc_weightThresholds, weightTotal, numVariations, &seed);

		//plot the result
		plot(renderTexture, p, c, matView, texWidth, texHeight);
	}

	if (iterations == 0)
	{
		//if there weren't any iterations, still want to draw where the point was
		plot(renderTexture, p, c, matView, texWidth, texHeight);
	}
}
);

std::string strRenderPostProcess = KERNEL_R_STRING(
kernel void renderPostProcess(global float4* renderTexture, global uchar4* processedRenderTexture, float gamma,
	float brightness, uchar renderTransparancy, uint numPixels)
{
	//apply post processing (gamma, brightness, float -> byte). this is done in fragment shader for preview.

	uint i = get_global_id(0);
	if (i >= numPixels) return;

	float4 pix = renderTexture[i];

	float alphaScale = log10(pix.w) / pix.w;
	pix = brightness * alphaScale * pix;
	pix = (float4)(pow(pix.xyz, (float3)(1.0f / gamma)), pix.w);
	if (!renderTransparancy)
	{
		float3 background = (float3)(0.0f);
		pix = (float4)(pix.xyz * pix.w + background, 1.0f);
	}

	pix = clamp(pix, 0.0f, 1.0f);
	pix *= 255.0f;
	uchar4 pixFinal = convert_uchar4(pix);
	processedRenderTexture[i] = pixFinal;
}
);

    std::string fullKernelSource =
		strAtomicAddFloat +
		strMat4MulVec4 +
		strRNG +
		strSierpinskiTriangle +
		strMengerSponge +
		strVariations +
		strF +
		strPlot +
		strProduceSamples +
		strRenderPostProcess;
	
    return strPreProc + formatKernelString(fullKernelSource);
}