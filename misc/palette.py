import json

class Palette(object):
    def __init__(self, palette_file):
        with open(palette_file, 'r') as file:
            self.palette = json.loads(file.read())

        self.paletteLength = len(self.palette)
        self.startOffset = 0

    @property
    def rotation(self):
        return self.startOffset

    @rotation.setter
    def rotation(self, n):
        self.startOffset = n

    def getKeyColor(self, key, level):
        index = self.startOffset + key # start from C
        return list(
            map(
                lambda value: round(level * value),
                self.palette[index % self.paletteLength]
            )
        )

    def getKeyColors(self, levels):
        levelsNum = len(levels)
        keyColors = []

        for key in range(levelsNum):
            keyColors.append(self.getKeyColor(key, levels[key]))

        return keyColors