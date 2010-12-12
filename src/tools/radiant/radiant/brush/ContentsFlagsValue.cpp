#include "ContentsFlagsValue.h"

#include "FaceShader.h"

namespace {
const unsigned int BRUSH_DETAIL_FLAG = 27;
const unsigned int BRUSH_DETAIL_MASK = (1 << BRUSH_DETAIL_FLAG);
}

ContentsFlagsValue::ContentsFlagsValue () :
	m_surfaceFlags(0), m_contentFlags(0), m_value(0), m_specified(false), m_surfaceFlagsDirty(0),
			m_contentFlagsDirty(0), m_markDirty(0), m_valueDirty(false), m_firstValue(true)
{
}
ContentsFlagsValue::ContentsFlagsValue (int surfaceFlags, int contentFlags, int value, bool specified,
		int surfaceFlagsDirty, int contentFlagsDirty, bool valueDirty) :
	m_surfaceFlags(surfaceFlags), m_contentFlags(contentFlags), m_value(value),
			m_specified(specified), m_surfaceFlagsDirty(surfaceFlagsDirty), m_contentFlagsDirty(contentFlagsDirty),
			m_markDirty(0), m_valueDirty(valueDirty), m_firstValue(true)
{
}

void ContentsFlagsValue::assignMasked (const ContentsFlagsValue& other)
{
	const unsigned int unchangedContentFlags = (m_contentFlags & other.m_contentFlagsDirty);
	const unsigned int changedContentFlags = (other.m_contentFlags & (~other.m_contentFlagsDirty));
	const unsigned int unchangedSurfaceFlags = (m_surfaceFlags & other.m_surfaceFlagsDirty);
	const unsigned int changedSurfaceFlags = (other.m_surfaceFlags & (~other.m_surfaceFlagsDirty));
	const int value = m_value;

	*this = other;

	m_contentFlags = unchangedContentFlags | changedContentFlags;
	m_surfaceFlags = unchangedSurfaceFlags | changedSurfaceFlags;

	// if it's not exactly clear which value should be taken here (multiple components
	// are selected that have different value settings) we just restore the original one
	if (m_valueDirty)
		m_value = value;
	m_valueDirty = false;
}

bool ContentsFlagsValue::isSpecified () const
{
	return m_specified;
}

void ContentsFlagsValue::setSpecified (bool specified)
{
	m_specified = specified;
}

int ContentsFlagsValue::getContentFlags () const
{
	return m_contentFlags;
}

int ContentsFlagsValue::getSurfaceFlags () const
{
	return m_surfaceFlags;
}

int ContentsFlagsValue::getValue () const
{
	return m_value;
}

void ContentsFlagsValue::setContentFlags (int contentFlags)
{
	m_contentFlags = contentFlags;
}

void ContentsFlagsValue::setSurfaceFlags (int surfaceFlags)
{
	m_surfaceFlags = surfaceFlags;
}

void ContentsFlagsValue::setValue (int value)
{
	m_value = value;
}

void ContentsFlagsValue::setDirty (bool dirty)
{
	m_markDirty = dirty;
}

bool ContentsFlagsValue::isDirty () const
{
	return m_markDirty;
}

int ContentsFlagsValue::getLevelFlags () const
{
	return (m_contentFlags >> 8) & 255;
}

void ContentsFlagsValue::setLevelFlags (int levelFlags)
{
	int levels = levelFlags;
	levels &= 255;
	levels <<= 8;

	m_contentFlags &= levels;
	m_contentFlags |= levels;
}

void ContentsFlagsValue::moveLevelUp ()
{
	int levels = getLevelFlags();
	levels <<= 1;
	if (!levels)
		levels = 1;
	setLevelFlags(levels);
}

void ContentsFlagsValue::moveLevelDown ()
{
	int levels = getLevelFlags();
	const int newLevel = levels >> 1;
	if (newLevel != (newLevel & 255))
		return;
	levels >>= 1;
	setLevelFlags(levels);
}

bool ContentsFlagsValue::isDetail () const
{
	return m_contentFlags & BRUSH_DETAIL_MASK;
}

void ContentsFlagsValue::setDetail (bool detail)
{
	if (detail && !isDetail()) {
		m_contentFlags |= BRUSH_DETAIL_MASK;
	} else if (!detail && isDetail()) {
		m_contentFlags &= ~BRUSH_DETAIL_MASK;
	}
}

bool ContentsFlagsValue::isValueDirty () const
{
	return m_valueDirty;
}

int ContentsFlagsValue::getContentFlagsDirty () const
{
	return m_contentFlagsDirty;
}

int ContentsFlagsValue::getSurfaceFlagsDirty () const
{
	return m_surfaceFlagsDirty;
}

/**
 * @brief Get the content and surface flags from a given face
 * @note Also generates a diff bitmask of the flag variable given to store
 * the flag values in and the current face flags
 * @param[in,out] flags The content and surface flag container
 * @sa ContentsFlagsValue_assignMasked
 */
void ContentsFlagsValue::mergeFlags (ContentsFlagsValue& flags)
{
	// rescue the mark dirty value and old dirty flags
	setDirty(flags.isDirty());
	m_surfaceFlagsDirty = flags.m_surfaceFlagsDirty;
	m_contentFlagsDirty = flags.m_contentFlagsDirty;

	if (isDirty()) {
		// Figure out which buttons are inconsistent
		m_contentFlagsDirty |= (flags.getContentFlags() ^ m_contentFlags);
		m_surfaceFlagsDirty |= (flags.getSurfaceFlags() ^ m_surfaceFlags);
	}
	setDirty(true);
	// preserve dirty state, don't mark dirty if we only select one face / first face (value in old flags is 0, own value could differ)
	if (flags.m_valueDirty || (getValue() != flags.getValue() && !flags.m_firstValue)) {
		m_valueDirty = true;
	}
	m_firstValue = false;
}
