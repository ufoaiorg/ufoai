#!/usr/bin/python

def main():
    # 1. update images
    # TODO

    # 2. generate html
    from yawp import static

    static.generate(Root(), 'generated')

if __name___ == '__main__':
    main()
