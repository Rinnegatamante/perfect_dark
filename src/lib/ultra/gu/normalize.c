#include <ultra64.h>

#ifdef __vita__
#include <math_neon.h>
#endif

void guNormalize(f32 *xyz)
{
#ifdef __vita__
	normalize3_neon(xyz, xyz);
#else
	f32 hyp = sqrtf(xyz[0] * xyz[0] + xyz[1] * xyz[1] + xyz[2] * xyz[2]);

	if (hyp > 0.0f) {
		f32 hyp2 = 1.0f / hyp;
		xyz[0] *= hyp2;
		xyz[1] *= hyp2;
		xyz[2] *= hyp2;
	} else {
		xyz[0] = 0.0f;
		xyz[1] = 0.0f;
		xyz[2] = 1.0f;
	}
#endif
}
