import json

class Palette(object):
    def __init__(self, palette_file):
        self.startOffset = 0
        with open(palette_file, 'r') as file:
            self.palette = json.loads(file.read())

    @property
    def rotation(self):
        return self.startOffset

    @rotation.setter
    def rotation(self, n):
        self.startOffset = n

    def getKeyColors(self, levels):
        levelsNum = len(levels)
        paletteLength = len(self.palette)
        keyColors = []

        for key in range(levelsNum):
            level = levels[key]
            index = self.startOffset + key # start from C
            keyColors.append(
                list(
                    map(
                        lambda value: round(level * value),
                        self.palette[index % paletteLength]
                    )
                )
            )

        return keyColors