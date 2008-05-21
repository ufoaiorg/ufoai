package net.ufoai.sf.maputils;

/**
 * LevelFlags.java
 * Created on 19 April 2008, 07:14
 * @author andy buckle
 */
public class ContentFlags {

	/** levels are 0-7, corresponding to 1 to 8 in the game. */
	public final static int MIN_LEVEL = 0, MAX_LEVEL = 7;
	public static final int STEPON = 0x40000000, ACTORCLIP = 0x10000, WEAPONCLIP = 0x2000000;
	public static final int STEPON_ACTORCLIP_WEAPONCLIP = STEPON + ACTORCLIP + WEAPONCLIP;
	private static int[] levelFlagsAbove = new int[] {0xFF00, 0xFE00, 0xFC00, 0xF800, 0xF000, 0xE000, 0xC000, 0x8000, 0x0000};

	private static int[] levelFlags = new int[] {0x0000, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000};

	private static int levelFlagMask = 0xFF00;

	private static final int FROM_LEVEL = 0, FROM_FLAGS = 1;

	private int thisFlags;

	/** @param integer must either be the level of the brush for which the flag
	                   set is required or an integer with the flag packed in
	    @param from    one of FROM_FLAGS or FROM_LEVEL, describes if the integer
	                   arg should be interpreted as a level or st of flags.*/
	private ContentFlags (int integer, int from) {
		switch (from) {
		case FROM_LEVEL:
			if (integer > MAX_LEVEL || integer < MIN_LEVEL)
				throw new IllegalArgumentException ("level should be between " + MIN_LEVEL + " and " + MAX_LEVEL + ". it is " + integer);
			thisFlags = levelFlagsAbove[integer];
			break;
		case FROM_FLAGS:
			thisFlags = integer;
			break;
		default:
			throw new RuntimeException ("\"from\" arg should be FROM_FLAGS or FROM_LEVEL ");
		}
	}

	/** @return the levelflags part masked out of this contentflags arg. */
	public int mask () {
		return thisFlags & levelFlagMask;
	}

	public static ContentFlags level (int level) {
		return new ContentFlags (level, FROM_LEVEL);
	}

	public static ContentFlags flags (int flags) {
		return new ContentFlags (flags, FROM_FLAGS);
	}

	public String getDescription() {
		String desc = "" + thisFlags + "(";
		for (int i = 1;i < levelFlags.length;i++) {
			boolean flagSetAtLeveli = (thisFlags & levelFlags[i]) != 0 ;
			desc += "" + i + ":" + (flagSetAtLeveli ? "1" : "0") + "," ;
		}
		desc = desc.substring (0, desc.length() - 1) + ")";
		return desc;
	}

	public String toString() {
		return "" + thisFlags;
	}

	/** Only compares the bits that are relevant for levelFlags.
	 *  @return true if the LevelFlags represent the same set of flags. */
	public boolean equals (Object o) {
		ContentFlags lf = (ContentFlags) o;
		return (this.thisFlags & levelFlagMask) == (lf.thisFlags & levelFlagMask);
	}

	/** changes the flags stored in the int within this class. replaces them with
	 *  the levelflags in newFlags. Other flags set are not changed. */
	public void mergeNewFlags (ContentFlags newFlags) {
		thisFlags = (thisFlags & ~levelFlagMask) | newFlags.thisFlags;
	}

	/** test code */
	public static void main (String[] args) {
		ContentFlags lf1 = ContentFlags.flags (64512);
		ContentFlags lf2 = ContentFlags.flags (64512 + 2);
		System.out.println (lf1.getDescription() );
		System.out.println (lf2.getDescription() );
		System.out.println ("equal?:" + lf1.equals (lf2) );
		System.out.println ("");
		ContentFlags lf3 = ContentFlags.flags (4 + 0x800);
		System.out.println (lf3.getDescription() );
		System.out.println ("lf3 extra flag (non-level) " + ( (lf3.thisFlags & 4) != 0) );
		lf3.mergeNewFlags (lf2);
		System.out.println (lf3.getDescription() );
		System.out.println ("lf3 extra flag (non-level) " + ( (lf3.thisFlags & 4) != 0) );
		System.out.println ("lf2 extra flag (non-level) " + ( (lf2.thisFlags & 4) != 0) );
	}

	/** returns true if this set of flags indicates actorclip, weaponclip or stepon. */
	public boolean isActorclipWeaponClipOrStepon() {
		return (thisFlags & STEPON_ACTORCLIP_WEAPONCLIP) != 0 ;
	}


	/** trims the level to the allowed range. if the argument is lower than MIN_LEVEL then
	 *  MIN_LEVEL is returned. if the argument is greater than MAX_LEVEL then MAX_LEVEL is returned.*/
	public static int clipLevel (int level) {
		return level < MIN_LEVEL ? MIN_LEVEL : (level > MAX_LEVEL ? MAX_LEVEL : level) ;
	}
	
	/** examines the levelflags and returns the level for nodraw (and possibly
	 *  other optimisation purposes) */
	public int getLevelForOptimisation(){
	    for (int i = 1;i < levelFlags.length;i++) {
		if( (thisFlags & levelFlags[i]) != 0 ) return i;
	    }
	    /* no flags set. -1 defualt is safe as other brushes will be considered to 
	     * be on proper levels and no nodraws will be set */
	    return -1;
	}

}
