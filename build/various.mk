# sync sourceforget.net svn to local svn dir
LOCAL_SVN_DIR=/var/lib/svn
rsync:
	export RSYNC_PROXY=rsync-svn.sourceforge.net:80; rsync -a rsync-svn-u::svn/ufoai/* $(LOCAL_SVN_DIR)

# generate doxygen docs
docs:
	doxygen src/docs/doxyall

# debian packages
# build this first - install ufoai-tools and then build debdata
debbinary:
	cd src; debuild binary

debdata:
	cd base; debuild binary

deb: debbinary debdata
