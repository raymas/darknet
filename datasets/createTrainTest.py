import os

file_location = os.path.dirname(os.path.realpath(__file__))
rootDataset = os.path.join(file_location, "VOC_PASCAL")
rootTrain = os.path.join(rootDataset, "Train")
rootTest = os.path.join(rootDataset, "Test")

TARGET = ".jpg"

def getImages(root) :
  out = []
  for path, subdirs, files in os.walk(root):
    for name in files:
      pathFile = os.path.join(path, name)
      dirFile = os.path.join(path)
      if TARGET in pathFile :
        out.append(pathFile)

  return out


def writeFile(path, name, buf) :
  with open(os.path.join(path, name), "w") as f :
    f.write(buf)
  f.close()


train = getImages(rootTrain)
test = getImages(rootTest)

writeFile(rootDataset, "train.txt", "\n".join(train))
writeFile(rootDataset, "test.txt", "\n".join(test))
