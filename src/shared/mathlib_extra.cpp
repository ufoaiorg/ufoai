/**
 * @file mathlib_extra.c
 * @brief Special, additional math algorithms for floating-point values
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mathlib_extra.h"
#include <math.h>

/**
 * @brief Takes a floating-point value (double) between 0.0 and 1.0 and returns a new value within the same range,
 * with output values skewed or "leaning" upward (toward 1.0) in a curve with a controlled shape.
 * @param[in] fpVal Initial floating-Point value (double) to be altered.
 * @param[in] mEffect How extreme the effect is (the shape of the curve).  A value of 0.0 is no curve or effect at all,
 * while a value of 1.0 is for maximum effect (very extreme).
 * @return The new, altered value.
 */
double FpCurveUp (double fpVal, double mEffect)
{
	/* If there isn't any "curve" to skew fpVal with (flat, straight line), we can stop right here (no change). */
	if (mEffect == 0.0)
		return fpVal;

	/* Safe values only please, to avoid breaking things */
	fpVal = fmin(DENORM_INV, fmax(DENORM, fpVal));
	mEffect = fmin(DENORM_INV, fmax(DENORM, mEffect));

	fpVal = ((2.0 - mEffect) * fpVal) / ((1.0 - mEffect) + fpVal);

	return fpVal;
}

/**
 * @brief Takes a floating-point value (double) between 0.0 and 1.0 and returns a new value within the same range,
 * with output values skewed or "leaning" downward (toward 0.0) in a curve with a controlled shape.
 * @param[in] fpVal Initial floating-Point value (double) to be altered.
 * @param[in] mEffect How extreme the effect is (the shape of the curve).  A value of 0.0 is no curve or effect at all,
 * while a value of 1.0 is for maximum effect (very extreme).
 * @return The new, altered value.
 */
double FpCurveDn (double fpVal, double mEffect)
{
	/* If there isn't any "curve" to skew fpVal with (flat, straight line), we can stop right here (no change). */
	if (mEffect == 0.0)
		return fpVal;

	/* Safe values only please, to avoid breaking things */
	fpVal = fmin(DENORM_INV, fmax(DENORM, fpVal));
	mEffect = fmin(DENORM_INV, fmax(DENORM, mEffect));

	fpVal = 1.0 - fpVal;
	fpVal = ((2.0 - mEffect) * fpVal) / ((1.0 - mEffect) + fpVal);
	fpVal = 1.0 - fpVal;

	return fpVal;
}

/**
 * @brief Takes a floating-point value (double) between 0.0 and 1.0 and returns a new value within the same range,
 * with output values skewed or "leaning" upward (toward 1.0) in a curve with a controlled shape that is more extreme
 * closer to 0.0 and tapers off closer to 1.0, resulting in a curve that bends into a special shape.
 * @param[in] fpVal Initial floating-Point value (double) to be altered.
 * @param[in] mEffect How extreme the effect is (the shape of the curve).  A value of 0.0 is no curve or effect at all,
 * while a value of 1.0 is for maximum effect (very extreme).
 * @return The new, altered value.
 */
double FpCurveUpRs (double fpVal, double mEffect)
{
	/* If there isn't any "curve" to skew fpVal with (flat, straight line), we can stop right here (no change). */
	if (mEffect == 0.0)
		return fpVal;

	/* Safe values only please, to avoid breaking things */
	fpVal = fmin(DENORM_INV, fmax(DENORM, fpVal));
	mEffect = fmin(DENORM_INV, fmax(DENORM, mEffect));

	fpVal = (((2.0 - mEffect) * fpVal) / ((1.0 - mEffect) + fpVal)) + fpVal;
	fpVal *= 0.50;

	return fpVal;
}

/**
 * @brief Takes a floating-point value (double) between 0.0 and 1.0 and returns a new value within the same range,
 * with output values skewed or "leaning" downward (toward 0.0) in a curve with a controlled shape that is more extreme
 * closer to 0.0 and tapers off closer to 1.0, resulting in a curve that bends into a special shape.
 * @param[in] fpVal Initial floating-Point value (double) to be altered.
 * @param[in] mEffect How extreme the effect is (the shape of the curve).  A value of 0.0 is no curve or effect at all,
 * while a value of 1.0 is for maximum effect (very extreme).
 * @return The new, altered value.
 */
double FpCurveDnRs (double fpVal, double mEffect)
{
	/* If there isn't any "curve" to skew fpVal with (flat, straight line), we can stop right here (no change). */
	if (mEffect == 0.0)
		return fpVal;

	/* Safe values only please, to avoid breaking things */
	fpVal = fmin(DENORM_INV, fmax(DENORM, fpVal));
	mEffect = fmin(DENORM_INV, fmax(DENORM, mEffect));

	fpVal = 1.0 - fpVal;
	fpVal = (((2.0 - mEffect) * fpVal) / ((1.0 - mEffect) + fpVal)) + fpVal;
	fpVal *= 0.50;
	fpVal = 1.0 - fpVal;

	return fpVal;
}

/**
 * @brief Takes a floating-point value (double) between 0.0 and 1.0 and returns a new value within the same range,
 * with output values skewed or "leaning" inward toward a given center point value in a curve with a controlled shape.
 * @param[in] fpVal Initial floating-Point value (double) to be altered.
 * @param[in] mEffect How extreme the effect is (the shape of the curve).  A value of 0.0 is no curve or effect at all,
 * while a value of 1.0 is for maximum effect (very extreme).
 * @param[in] cntPnt The center point (a value between 0.0 and 1.0) that fpVal should "lean" or skew to.
 * @return The new, altered value.
 */
double FpCurve1D_u_in (double fpVal, double mEffect, double cntPnt)
{
	/* If there isn't any "curve" to skew fpVal with (flat, straight line), we can stop right here (no change). */
	if (mEffect == 0.0)
		return fpVal;

	/* Safe values only please, to avoid breaking things */
	fpVal = fmin(DENORM_INV, fmax(DENORM, fpVal));
	mEffect = fmin(DENORM_INV, fmax(DENORM, mEffect));
	cntPnt = fmin(DENORM_INV, fmax(DENORM, cntPnt));

	if (fpVal > cntPnt) {
		double fpVal_mod = (fpVal - cntPnt) / (1.0 - cntPnt);

		fpVal_mod = FpCurveDn(fpVal_mod, mEffect);
		fpVal_mod *= 1.0 - cntPnt;
		fpVal_mod += cntPnt;
		return fpVal_mod;
	}
	if (fpVal < cntPnt) {
		double fpVal_mod = (cntPnt - fpVal) / cntPnt;

		fpVal_mod = FpCurveDn(fpVal_mod, mEffect);
		fpVal_mod *= cntPnt;
		fpVal_mod = cntPnt - fpVal_mod;
		return fpVal_mod;
	}
	/* fpVal = cntPnt, so no change */
	return fpVal;
}

/**
 * @brief Takes a floating-point value (double) between 0.0 and 1.0 and returns a new value within the same range,
 * with output values skewed or "leaning" outward away from a given center point value in a curve with a controlled shape.
 * The value will curve toward either 1.0 or 0.0, depending on which one would NOT mean crossing the center point.
 * @param[in] fpVal Initial floating-Point value (double) to be altered.
 * @param[in] mEffect How extreme the effect is (the shape of the curve).  A value of 0.0 is no curve or effect at all,
 * while a value of 1.0 is for maximum effect (very extreme).
 * @param[in] cntPnt The center point (a value between 0.0 and 1.0) that fpVal should "lean" or skew away from.
 * @return The new, altered value.
 */
double FpCurve1D_u_out (double fpVal, double mEffect, double cntPnt)
{
	/* If there isn't any "curve" to skew fpVal with (flat, straight line), we can stop right here (no change). */
	if (mEffect == 0.0)
		return fpVal;

	/* Safe values only please, to avoid breaking things */
	fpVal = fmin(DENORM_INV, fmax(DENORM, fpVal));
	mEffect = fmin(DENORM_INV, fmax(DENORM, mEffect));
	cntPnt = fmin(DENORM_INV, fmax(DENORM, cntPnt));

	if (fpVal > cntPnt) {
		double fpVal_mod = (fpVal - cntPnt) / (1.0 - cntPnt);

		fpVal_mod = FpCurveUp(fpVal_mod, mEffect);
		fpVal_mod *= 1.0 - cntPnt;
		fpVal_mod += cntPnt;
		return fpVal_mod;
	}
	if (fpVal < cntPnt) {
		double fpVal_mod = (cntPnt - fpVal) / cntPnt;

		fpVal_mod = FpCurveUp(fpVal_mod, mEffect);
		fpVal_mod *= cntPnt;
		fpVal_mod = cntPnt - fpVal_mod;
		return fpVal_mod;
	}
	/* fpVal = cntPnt, so no change */
	return fpVal;
}

/**
 * @brief Takes a floating-point value (double) between -1.0 and +1.0 and returns a new value within the same range,
 * with output values skewed or "leaning" outward away from 0.0.
 * The value will curve toward either -1.0 or +1.0, depending on which one would NOT mean crossing 0.0.
 * @param[in] fpVal Initial floating-Point value (double) to be altered.  May range from -1.0 to +1.0.
 * @param[in] mEffect How extreme the effect is (the shape of the curve).  A value of 0.0 is no curve or effect at all,
 * while a value of 1.0 is for maximum effect (very extreme).
 * @return The new, altered value.
 */
double FpCurve1D_s_out (double fpVal, double mEffect)
{
	/* If there isn't any "curve" to skew fpVal with (flat, straight line), we can stop right here (no change). */
	if (mEffect == 0.0)
		return fpVal;

	/* Safe values only please, to avoid breaking things */
	fpVal = fmin(DENORM_INV, fmax(DENORM, fpVal));
	mEffect = fmin(DENORM_INV, fmax(DENORM, mEffect));

	fpVal = ((2.0 - mEffect) * fpVal) / ((1.0 - mEffect) + fabs(fpVal));

	return fpVal;
}

/**
 * @brief Takes a (float) of any value and outputs a value from -1.f to +1.f, along a curve, so that as the input
 * value gets farther from 0.0f it slows down and never quite gets to +1.f or -1.f.
 * @param[in] inpVal The original input value.
 * @param[in] hard The steepness or slope of the curve, valid values are 0.f to +inf.  Higher values mean a more rapid
 * approach of inpVal toward +1.f or -1,f.
 * @return The altered value, between -1.f and 1.f.
 */
float FpUcurve_f(const float inpVal, const float hard)
{
	return (float) ( inpVal >= 0.f ? (inpVal * (hard+inpVal) / (1.f + (hard*inpVal) + (inpVal*inpVal))) :
					(inpVal * (hard-inpVal) / (1.f - (hard*inpVal) + (inpVal*inpVal))) );
}

/**
 * @brief Takes a (float) of any value and outputs a value from -1.f to +1.f, along a curve, so that as the input
 * value gets farther from 0.0f it slows down and never quite gets to +1.f or -1.f.
 * @param[in] inpVal The original input value.
 * @param[in] hard The steepness or slope of the curve, valid values are 0.f to +inf.  Higher values mean a more rapid
 * approach of inpVal toward +1.f or -1,f.
 * @return The altered value, between -1.f and 1.f.
 */
double FpUcurve_d(const double inpVal, const double hard)
{
	return (double) ( inpVal >= 0.0 ? (inpVal * (hard+inpVal) / (1.0 + (hard*inpVal) + (inpVal*inpVal))) :
					(inpVal * (hard-inpVal) / (1.0 - (hard*inpVal) + (inpVal*inpVal))) );
}

/**
 * @brief Takes a (float) of any value and outputs a value from -1.f to +1.f, along a curve, so that as the input
 * value gets farther from 0.0f it slows down and never quite gets to +1.f or -1.f.
 * @param[in] inpVal The original input value.
 * @param[in] hard The steepness or slope of the curve, valid values are 0.f to +inf.  Higher values mean a more rapid
 * @param[in] scale An additional scaling factor that can affect the shape of the sloped curve of potential output values.
 * approach of inpVal toward +1.f or -1,f.
 * @return The altered value, between -1.f and 1.f.
 */
float FpUcurveSc_f(const float inpVal, const float hard, const float scale)
{
	return (float) ( inpVal >= 0.f ? (inpVal * (hard+inpVal) / (scale + (hard*inpVal) + (inpVal*inpVal))) :
					(inpVal * (hard-inpVal) / (scale - (hard*inpVal) + (inpVal*inpVal))) );
}

/**
 * @brief Takes a (float) of any value and outputs a value from -1.f to +1.f, along a curve, so that as the input
 * value gets farther from 0.0f it slows down and never quite gets to +1.f or -1.f.
 * @param[in] inpVal The original input value.
 * @param[in] hard The steepness or slope of the curve, valid values are 0.f to +inf.  Higher values mean a more rapid
 * @param[in] scale An additional scaling factor that can affect the shape of the sloped curve of potential output values.
 * approach of inpVal toward +1.f or -1,f.
 * @return The altered value, between -1.f and 1.f.
 */
double FpUcurveSc_d(const double inpVal, const double hard, const double scale)
{
	return (double) ( inpVal >= 0.0 ? (inpVal * (hard+inpVal) / (scale + (hard*inpVal) + (inpVal*inpVal))) :
					(inpVal * (hard-inpVal) / (scale - (hard*inpVal) + (inpVal*inpVal))) );
}
