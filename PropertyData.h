#ifndef PROPERTYDATA_H
#define PROPERTYDATA_H

#include <QFont>


struct PageSize
{
    int PageHeight;
    int PageWidth;
    int FeedPageHeight;
    int FeedPageWidth;
    int SubjectPageWidth;
};

struct Pagination
{
    int DTSideDistance;
    int DTBottomDistance;
    int DTIntervalDistance;

    //"FontSize", "Height"
    std::tuple<int, int> Number;

    //"FontSize", "Height"
    std::tuple<int, int> Text;

    //"Width": ,"Height"
    std::tuple<int, int> Line;
};

struct DividingLine
{
    int X;
    int Width;
    int Height;
    QString Color;
};

struct FeedFonts
{
    //Property FontName and FontSize(pixel size)
    QFont MarkFont;
    QFont TitleFont;
    QFont ContentFont;
    QFont QuoteFont;
    QFont TagFont;
};

struct SubjectFonts
{
    //Property FontName and FontSize(pixel size)
    QFont TitleFont;
    QFont ContentFont;
};

struct WishFonts
{
    //Property FontName and FontSize(pixel size)
    QFont TitleFont;
    QFont SignatureFont;
};

struct DirectoryFonts
{
    //Property FontName and FontSize(pixel size)
    QFont HeadEntryFont;
    QFont SubEntryFont;
};

struct ContentFont
{
    //Property FontName and FontSize(pixel size)
    QFont FontName;
    QFont FontSize;
};



#endif // PROPERTYDATA_H
