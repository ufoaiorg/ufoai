#ifndef CONTENTSFLAGSVALUE_H_
#define CONTENTSFLAGSVALUE_H_

class ContentsFlagsValue
{
	private:
		int m_surfaceFlags;
		int m_contentFlags;
		int m_value;
		bool m_specified;
		int m_surfaceFlagsDirty;
		int m_contentFlagsDirty;
		int m_markDirty;
		bool m_valueDirty;
		bool m_firstValue;// marker for value diff calculation. see GetFlags for use

	public:
		ContentsFlagsValue ();
		ContentsFlagsValue (int surfaceFlags, int contentFlags, int value, bool specified, int surfaceFlagsDirty = 0,
				int contentFlagsDirty = 0, bool valueDirty = false);

		void assignMasked (const ContentsFlagsValue& other);

		bool isSpecified () const;
		void setSpecified (bool specified);

		int getContentFlags () const;
		int getContentFlagsDirty () const;
		void setContentFlags (int contentFlags);

		int getSurfaceFlags () const;
		int getSurfaceFlagsDirty () const;
		void setSurfaceFlags (int surfaceFlags);

		int getValue () const;
		void setValue (int value);

		bool isValueDirty () const;
		bool isDirty () const;
		void setDirty (bool dirty);

		int getLevelFlags () const;
		void setLevelFlags (int levelFlags);

		void moveLevelUp ();
		void moveLevelDown ();

		bool isDetail () const;
		void setDetail (bool detail);

		/**
		 * @brief Get the content and surface flags from a given face
		 * @note Also generates a diff bitmask of the flag variable given to store
		 * the flag values in and the current face flags
		 * @param[in,out] flags The content and surface flag container
		 * @sa ContentsFlagsValue_assignMasked
		 */
		void mergeFlags (ContentsFlagsValue& flags);
}; // class ContentsFlagsValue

#endif /*CONTENTSFLAGSVALUE_H_*/
