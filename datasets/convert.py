import os

file_location = os.path.dirname(os.path.realpath(__file__))
images_root = "VOC_PASCAL"
root = os.path.join(file_location, images_root)



FROM=".ppm.tif"
TO=".jpg"

for path, subdirs, files in os.walk(root):
    for name in files:
        pathFile = os.path.join(path, name)
        dirFile = os.path.join(path)
        if FROM in pathFile :
            cmd = "convert " + str(pathFile) + " " + str(pathFile[:(-1) * len(FROM)]) + str(TO)
            # print(cmd)
            os.system(cmd)

cmd = "find " + str(root) +" -name \"*" + str(FROM) + "\" -exec rm -r {} \;"
os.system(cmd)
