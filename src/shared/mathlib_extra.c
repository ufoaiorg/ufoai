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
float XMath_CurveUnlFixed_f (const float inpVal, const float hard)
{
	return (float) ( (inpVal >= 0.f) ? (inpVal * (hard+inpVal) / (1.f + (hard*inpVal) + (inpVal*inpVal)))
					: (inpVal * (hard-inpVal) / (1.f - (hard*inpVal) + (inpVal*inpVal))) );
}

/**
 * @brief Takes a (float) of any value and outputs a value from -1.f to +1.f, along a curve, so that as the input
 * value gets farther from 0.0f it slows down and never quite gets to +1.f or -1.f.
 * @param[in] inpVal The original input value.
 * @param[in] hard The steepness or slope of the curve, valid values are 0.f to +inf.  Higher values mean a more rapid
 * approach of inpVal toward +1.f or -1,f.
 * @return The altered value, between -1.f and 1.f.
 */
double XMath_CurveUnlFixed_d (const double inpVal, const double hard)
{
	return (double) ( (inpVal >= 0.0) ? (inpVal * (hard+inpVal) / (1.0 + (hard*inpVal) + (inpVal*inpVal)))
					: (inpVal * (hard-inpVal) / (1.0 - (hard*inpVal) + (inpVal*inpVal))) );
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
float XMath_CurveUnlScaled_f (const float inpVal, const float hard, const float scale)
{
	return (float) ( (inpVal >= 0.f) ? (inpVal * (hard+inpVal) / (scale + (hard*inpVal) + (inpVal*inpVal)))
					: (inpVal * (hard-inpVal) / (scale - (hard*inpVal) + (inpVal*inpVal))) );
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
double XMath_CurveUnlScaled_d (const double inpVal, const double hard, const double scale)
{
	return (double) ( (inpVal >= 0.0) ? (inpVal * (hard+inpVal) / (scale + (hard*inpVal) + (inpVal*inpVal)))
					: (inpVal * (hard-inpVal) / (scale - (hard*inpVal) + (inpVal*inpVal))) );
}

/**
 * @brief Initializes (or resets) a xMathRcBufferF_t structure, this must be done at least once
 * for any xMathRcBufferF_t instance before it can be used.
 *
 * @param[in] rcbuff The xMathRcBufferF_t instance to set up or reset.
 * @param[in] inpRate The number of times per second that each "tick" is expected to run.
 * @param[in] inpPass A ratio value from 0.f to 1.f which determines how much effect the filter has.
 * This means that a value of 0.5f sets the filter pass to half the rate or "inpRate" value.
 */
void XMath_RcBuffInit (xMathRcBufferF_t *rcbuff, const float inpRate, const float inpPass)
{
	rcbuff->rate = fmax(inpRate, (float) DENORM);
	rcbuff->passRate = fmin(1.f, fmax(inpPass, (float) DENORM));
	rcbuff->buffer = 0.f;
	rcbuff->input = 0.f;

	/* The rate factors are calculated with double precision, for better accuracy, before being stored in (float)s. */
	const double passRatio = (double) rcbuff->passRate * 0.50 * (double) rcbuff->rate;
	const double calculatedFactor = (double) ( exp(-2.0 * (double) M_PI * passRatio / (double) rcbuff->rate) );
	rcbuff->passFactorB = (float) calculatedFactor;
	rcbuff->passFactorA = (float) (1.0 - calculatedFactor);
}
/**
 * @brief Inputs the target (float) value that the output will gradually head toward.
 * Note that this can be set more than once, or even not at all, between each tick,
 * without causing issues.
 *
 * @param[in] rcbuff The filter instance to set the input for.
 * @param[in] inpVal The target value, to be input into the buffer.
 */
void XMath_RcBuffInput (xMathRcBufferF_t *rcbuff, const float inpVal)
{
	rcbuff->input = inpVal;
}
/**
 * @brief Gets the buffered output value of the ilter.
 * Note that this can be ran more than once, or even not at all, between each tick,
 * without causing issues.
 *
 * @param[in] rcbuff The filter instance to set the input for.
 * @return The buffered output value.
 */
float XMath_RcBuffGetOutput (const xMathRcBufferF_t *rcbuff)
{
	return rcbuff->buffer;
}
/**
 * @brief Runs the filter.  This needs to be called a number of times per second equal to the
 * rate value set when the filter was initialized, no more, no less.
 *
 * @param[in] rcbuff The filter instance to set the input for.
 * @note If this makes the program run too slow, please consider lowering the rate value when setting up the filter.
 */
void XMath_RcBuffTick (xMathRcBufferF_t *rcbuff)
{
	rcbuff->buffer = (float) (rcbuff->passFactorA*rcbuff->input + rcbuff->passFactorB*rcbuff->buffer + (float) DENORM);
}
/**
 * @brief Forces the buffer to a new value, essentially eliminating the usefulness of the filter temporarily,
 * by resetting the buffer without changing the rate or pass rate.  This is useful for certain situations, but if
 * this function is used too often on a filter it results in a waste of CPU usage because the filter is ran with each
 * tick but not really doing anything useful if it is always being reset.
 *
 * @param[in] rcbuff The filter instance to set the input for.
 * @param[in] inpForceVal The new value to reset and force the buffer to.  This can be 0.f or any other (float) value.
 * @note This also resets the input target value for the filter.  If you don't want this, you will need to call the
 * XMath_RcBuffInput() function afterward.
 */
void XMath_RcBuffForceBuffer (xMathRcBufferF_t *rcbuff, const float inpForceVal)
{
	rcbuff->buffer = inpForceVal;
	rcbuff->input = inpForceVal;
}
/**
 * @brief Changes the rate at which the filter will expect a tick per second.  This function does NOT
 * reset buffers or input values, so it can be used mid-stream while the buffer is in use.
 *
 * @param[in] rcbuff The xMathRcBufferF_t instance to update.
 * @param[in] inpRate The number of times per second that each "tick" is expected to run.
 * @note Please don't run this function on a buffer too often, escpecially if many buffers are in use at
 * the time, as it could really slow down the program.
 */
void XMath_RcBuffChangeRate (xMathRcBufferF_t *rcbuff, const float inpRate)
{
	rcbuff->rate = fmax(inpRate, (float) DENORM);

	/* The rate factors are calculated with double precision, for better accuracy, before being stored in (float)s. */
	const double passRatio = (double) rcbuff->passRate * 0.50 * (double) rcbuff->rate;
	const double calculatedFactor = (double) ( exp(-2.0 * (double) M_PI * passRatio / (double) rcbuff->rate) );
	rcbuff->passFactorB = (float) calculatedFactor;
	rcbuff->passFactorA = (float) (1.0 - calculatedFactor);
}
/**
 * @brief Changes the pass rate ratio, for how much effect is applied by the filter.  This function does NOT
 * reset buffers or input values, so it can be used mid-stream while the buffer is in use.
 *
 * @param[in] rcbuff The xMathRcBufferF_t instance to update.
 * @param[in] inpPass A ratio value from 0.f to 1.f which determines how much effect the filter has.
 * This means that a value of 0.5f sets the filter pass to half the rate or "inpRate" value.
 * @note Please don't run this function on a buffer too often, escpecially if many buffers are in use at
 * the time, as it could really slow down the program.
 */
void XMath_RcBuffChangePassRate (xMathRcBufferF_t *rcbuff, const float inpPass)
{
	rcbuff->passRate = fmin(1.f, fmax(inpPass, (float) DENORM));

	/* The rate factors are calculated with double precision, for better accuracy, before being stored in (float)s. */
	const double passRatio = (double) rcbuff->passRate * 0.50 * (double) rcbuff->rate;
	const double calculatedFactor = (double) ( exp(-2.0 * (double) M_PI * passRatio / (double) rcbuff->rate) );
	rcbuff->passFactorB = (float) calculatedFactor;
	rcbuff->passFactorA = (float) (1.0 - calculatedFactor);
}
