#ifndef OPENGLSTATEMAP_H_
#define OPENGLSTATEMAP_H_

class OpenGLStateMap: public OpenGLStateLibrary
{
		typedef std::map<std::string, OpenGLState> States;
		States m_states;
	public:
		virtual ~OpenGLStateMap ()
		{
			ASSERT_MESSAGE(m_states.empty(), "OpenGLStateMap::~OpenGLStateMap: not empty");
		}

		typedef States::iterator iterator;
		iterator begin ()
		{
			return m_states.begin();
		}
		iterator end ()
		{
			return m_states.end();
		}

		void getDefaultState (OpenGLState& state) const
		{
			state.constructDefault();
		}

		void insert (const std::string& name, const OpenGLState& state)
		{
			bool inserted = m_states.insert(States::value_type(name, state)).second;
			ASSERT_MESSAGE(inserted, "OpenGLStateMap::insert: " << name << " already exists");
		}
		void erase (const std::string& name)
		{
			std::size_t count = m_states.erase(name);
			ASSERT_MESSAGE(count == 1, "OpenGLStateMap::erase: " << name << " does not exist");
		}

		iterator find (const std::string& name)
		{
			return m_states.find(name);
		}
};

#endif /* OPENGLSTATEMAP_H_ */
