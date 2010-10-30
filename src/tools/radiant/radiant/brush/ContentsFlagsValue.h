#ifndef CONTENTSFLAGSVALUE_H_
#define CONTENTSFLAGSVALUE_H_

const unsigned int BRUSH_DETAIL_FLAG = 27;
const unsigned int BRUSH_DETAIL_MASK = (1 << BRUSH_DETAIL_FLAG);

class ContentsFlagsValue
{
	public:
		ContentsFlagsValue () :
			m_surfaceFlags(0), m_contentFlags(0), m_value(0), m_specified(false), m_surfaceFlagsDirty(0),
					m_contentFlagsDirty(0), m_markDirty(0), m_valueDirty(false), m_firstValue(true)
		{
		}
		ContentsFlagsValue (int surfaceFlags, int contentFlags, int value, bool specified, int surfaceFlagsDirty = 0,
				int contentFlagsDirty = 0, bool valueDirty = false) :
			m_surfaceFlags(surfaceFlags), m_contentFlags(contentFlags), m_value(value), m_specified(specified),
					m_surfaceFlagsDirty(surfaceFlagsDirty), m_contentFlagsDirty(contentFlagsDirty), m_markDirty(0),
					m_valueDirty(valueDirty), m_firstValue(true)
		{
		}

		int m_surfaceFlags;
		int m_contentFlags;
		int m_value;
		bool m_specified;
		int m_surfaceFlagsDirty;
		int m_contentFlagsDirty;
		int m_markDirty;
		bool m_valueDirty;
		bool m_firstValue;// marker for value diff calculation. see GetFlags for use

		inline void assignMasked (const ContentsFlagsValue& other)
		{
			unsigned int unchangedContentFlags = (m_contentFlags & other.m_contentFlagsDirty);
			unsigned int changedContentFlags = (other.m_contentFlags & (~other.m_contentFlagsDirty));
			unsigned int unchangedSurfaceFlags = (m_surfaceFlags & other.m_surfaceFlagsDirty);
			unsigned int changedSurfaceFlags = (other.m_surfaceFlags & (~other.m_surfaceFlagsDirty));
			int value = m_value;
			*this = other;
			m_contentFlags = unchangedContentFlags | changedContentFlags;
			m_surfaceFlags = unchangedSurfaceFlags | changedSurfaceFlags;
			if (m_valueDirty)
				m_value = value;
			m_valueDirty = false;
			//m_contentFlagsDirty = 0;
			//m_surfaceFlagsDirty = 0;
			//m_markDirty = 0;
		}
}; // class ContentsFlagsValue

#endif /*CONTENTSFLAGSVALUE_H_*/
