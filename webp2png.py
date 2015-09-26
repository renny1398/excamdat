#!/usr/bin/env python

import os
import sys
import glob
import cv2
import webm
import webm.libwebp
#from webm import libwebp

dir_delim = '/'

base_directory = None

def imread_webp(fn):
    f = open(fn, 'rb')
    if f is None:
        return None
    d = f.read()
    f.close()
    ret = webp.WebPDecodeBGRA(d)
    rgba = ret[0]
    width = ret[1]
    height = ret[2]
    img = cv2.CreateMat(height, width, cv2.CV_8UC4)
    img.data = d
    return img

if __name__ == '__main__':
    
    if len(sys.argv) > 1:
        base_directory = sys.argv[1].rstrip(dir_delim)
    else:
        base_directory = os.getcwd()

    print base_directory + dir_delim + '*.dzi'
    files = glob.glob(base_directory + dir_delim + '*.dzi')

    for fn in files:
        f = open(fn, 'r')
        print 'Parsing ' + fn
        signature = f.readline()
        if signature[0:3] != 'DZI':
            print 'Warning: ' + fn + ' is not DZI file.'
            continue
        str_size = f.readline().split(',')
        width = int(str_size[0])
        height = int(str_size[1])
        levels = int(f.readline())

        for level_no in range(0, levels):
            png_img = None
            str_colrow = f.readline().split(',')
            cols = int(str_colrow[0])
            rows = int(str_colrow[1])
            print cols, rows
            for row_no in range(0, rows):
                png_img_per_row = None
                webp_list = f.readline().split(',')
                for col_no in range(0, cols):
                    webp_fn = base_directory + '/tex/' + webp_list[col_no] + '.webp'
                    webp_fn = webp_fn.replace('\\', '/')
                    if png_img_per_row is None:
                        png_img_per_row = imread_webp(webp_fn)
                        if png_img_per_row is None:
                            print 'Falied to open ' + webp_fn
                            sys.exit()
                    else:
                        png_img_per_row = cv2.hconcat([png_img_per_row, imread_webp(webp_fn)])
                if png_img is None:
                    png_img = png_img_per_row
                else:
                    png_img = cv2.vconcat([png_img, png_img_per_row])
            # save without change the image size
            dest_fn = fn.replace('.dzi', '') + '_' + str(level_no) + '.png'
            print dest_fn
            cv2.imwrite(dest_fn, png_img)
            png_img = None
        f.close()
