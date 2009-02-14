import os
from os.path import join

from yawp import static, wrapper, Link
from yawp.static import Leaf

import collect
from collect import BASE_PATH, DATA, get_licenses

class Root(Leaf):
    @wrapper
    def index(self, content, path=''):
        content['sub'] = [Link(Root.index, {'path': join(path, d.name)}, name=d.name)
                          for d in DATA.find(path).dirs]
        content['path'] = path

        licenses = [(len(files), Link(Root.licenses, {'license': li, 'path': path}, name=str(li)))
                     for li, files in get_licenses(DATA.find(path)).iteritems()]
        licenses.sort(reverse=True, key=lambda x: x[0])
        content['licenses'] = licenses
        return content

    @wrapper
    def licenses(self, content, path, license):
        license = license if license != 'None' else None
        content['path'] = path
        content['license'] = str(license)
        content['files'] = [f for f in DATA.find(path).files_r if f.license == license]
        return content


if __name__ == '__main__':
    import yawp
    collect.update()
    yawp.quickstart(Root())
