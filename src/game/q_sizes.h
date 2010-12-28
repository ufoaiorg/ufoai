#ifndef Q_SIZES_H
#define Q_SIZES_H

/* Sizes of the eye-levels and bounding boxes for actors/units. */
#define EYE_STAND			15
#define EYE_CROUCH			3
#define PLAYER_STAND		20
#define PLAYER_CROUCH		5
#define PLAYER_DEAD			-12
#define PLAYER_MIN			-24
#define PLAYER_WIDTH		9

#define PLAYER_STANDING_HEIGHT		(PLAYER_STAND - PLAYER_MIN)
#define PLAYER_CROUCHING_HEIGHT		(PLAYER_CROUCH - PLAYER_MIN)

#if 0
#define EYE2x2_STAND		15
#define EYE2x2_CROUCH		3
#define PLAYER2x2_STAND		20
#define PLAYER2x2_CROUCH	5
#define PLAYER2x2_DEAD		-12		/**< @todo  This may be the only one needed next to PLAYER2x2_WIDTH. */
#define PLAYER2x2_MIN		-24
#endif
#define PLAYER2x2_WIDTH		18	/**< Width of a 2x2 unit. @todo may need some tweaks. */

/* field marker box */
#define BOX_DELTA_WIDTH		11
#define BOX_DELTA_LENGTH	11
#define BOX_DELTA_HEIGHT	27

#endif
