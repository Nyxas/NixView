#ifndef COLORMAP_HPP
#define COLORMAP_HPP

#include <QColor>
#include <vector>

class ColorMap
{
public:
    ColorMap();

    QColor next();

private:
    size_t current;
    std::vector<QColor> cmap;

};

#endif // COLORMAP_HPP
