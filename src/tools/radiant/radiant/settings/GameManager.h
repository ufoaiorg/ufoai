#ifndef GAMEDIALOG_H_
#define GAMEDIALOG_H_

class GameDescription;

namespace ui {

/*!
 standalone dialog for games selection, and more generally global settings
 */
class GameManager
{
	private:

		GameDescription* _currentGameDescription;

	public:

		virtual ~GameManager ();

		static GameManager& Instance ();

		void initialise ();

		GameDescription* getGameDescription ();
};

} // namespace ui

#endif
