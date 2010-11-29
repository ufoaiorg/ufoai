#include "itextures.h"
#include "container/hashfunc.h"
#include "container/cache.h"

class TexturesMap: public TexturesCache
{
	public:

		typedef TexturesCache Type;
		STRING_CONSTANT(Name, "*");

		TexturesCache* getTable ()
		{
			return this;
		}

	private:

		typedef std::pair<LoadImageCallback, std::string> TextureKey;

		class TextureKeyEqualNoCase
		{
			public:

				bool operator() (const TextureKey& key, const TextureKey& other) const
				{
					return key.first == other.first && string_equal_nocase(key.second, other.second);
				}
		};

		class TextureKeyHashNoCase
		{
			public:

				typedef hash_t hash_type;

				hash_t operator() (const TextureKey& key) const
				{
					return hash_combine(string_hash_nocase(key.second.c_str()), pod_hash(key.first));
				}
		};

		class TextureConstructor
		{
			private:

				TexturesMap* m_cache;

			public:

				explicit TextureConstructor (TexturesMap* cache);

				GLTexture* construct (const TextureKey& key);

				void destroy (GLTexture* texture);
		};

		typedef HashedCache<TextureKey, GLTexture, TextureKeyHashNoCase, TextureKeyEqualNoCase, TextureConstructor>
				TextureCache;
		TextureCache m_qtextures;
		TexturesCacheObserver* m_observer;
		std::size_t m_unrealised;

		LoadImageCallback defaultLoader () const;

	public:

		TexturesMap ();

		GLTexture* capture (const std::string& name);

		GLTexture* capture (const LoadImageCallback& loader, const std::string& name);

		void release (GLTexture* texture);

		void attach (TexturesCacheObserver& observer);

		void detach (TexturesCacheObserver& observer);

		void realise ();

		void unrealise ();

		bool realised ();
};
