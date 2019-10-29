import os
import threading
from time import sleep

validtypes = ["mp4", "mkv", "flv"]

def isValidExt(filename, extList):
    for x in extList:
        if x in filename:
            return True
    return False

def listvids(root="D:\\Anime\\"):
    files = []
    x = os.listdir(root)
    for a in x:
        if os.path.isfile(root + a):
            if isValidExt(a, validtypes):
                files.append(root + a)
        else:
            files.extend(listvids(root+a+"\\"))

    return files

READSIZE = 1048576*10
AllFilesData = []

def openThread(f):
    vidFile = open(f, "rb")
    vidFile.seek(0, 2)
    fileSize = vidFile.tell()
    vidFile.seek(0, 0)

    print("Reading " + f)
    pos = 0
    errors = []
    while pos < fileSize:
        toRead = min(fileSize-pos, READSIZE)
        print(f + ": reading " + str(pos) + " - " + str(pos + toRead))
        
        try:
            vidFile.read(toRead)
        except:
            print("Couldn't read " + str(pos) + " in \"" + f + "\"")
            errors.append(pos)
            break
        pos += toRead

    AllFilesData.append((f, errors))
    print("Finished reading " + f)
    vidFile.close()

def main():
    videoFiles = listvids()
    threads = []

    for a in range(len(videoFiles)):
        while len(threads) >= 2:
            #Wait till all threads exit
            for th in threads:
                th.join()
                threads.remove(th)
                break

        thX = threading.Thread(target=openThread, daemon=True, args=(videoFiles[a],))
        threads.append(thX)
        thX.start()

    print("Done")
    print(AllFilesData)

if __name__ == "__main__":
    main()