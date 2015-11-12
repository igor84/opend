# GGPlotD [![Build Status](https://travis-ci.org/BlackEdder/ggplotd.svg?branch=master)](https://travis-ci.org/BlackEdder/ggplotd)

GGPlotD is a plotting library for the D programming language. The design
is heavily inspired by ggplot2 for R, which is based on a general Grammar of
Graphics described by Leland Wilkinson. The library depends on cairo(D) for
the actual drawing. The library is designed to make it easy to build complex
plots from simple building blocks.

## Install

Easiest is to use dub and add the library to your dub configuration file,
which will automatically download the D dependencies. The library also
depends on cairo so you will need to have that installed. On ubuntu/debian
you can install cairo with:

``` 
sudo apt-get install libcairo2-dev 
```

### PDF and SVG support

We rely on cairo for PDF and SVG support, but by default cairoD disables
pdf and svg support. To enable it you need to add a local copy of cairoD
that dub can find:

```
git clone https://github.com/jpf91/cairoD.git
sed -i 's/PDF_SURFACE = false/PDF_SURFACE = true/g' cairoD/src/cairo/c/config.d
sed -i 's/SVG_SURFACE = false/SVG_SURFACE = true/g' cairoD/src/cairo/c/config.d
dub add-local cairoD
```

## Documentation

Documentation is automatically generated and put online under
http://blackedder.github.io/ggplotd/ggplotd.html . For example for the
available geom* functions see: http://blackedder.github.io/ggplotd/geom.html

### Examples

At version v0.1.0 we have basic support for simple plots.

![A noisy figure](http://blackedder.github.io/ggplotd/images/noise.png)
```D 
import ggplotd.ggplotd;

void main()
{
    import std.array : array;
    import std.math : sqrt;
    import std.algorithm : map;
    import std.range : repeat, iota;
    import std.random : uniform;
    // Generate some noisy data with reducing width
    auto f = (double x) { return x/(1+x); };
    auto width = (double x) { return sqrt(0.1/(1+x)); };
    auto xs = iota( 0, 10, 0.1 ).array;

    auto ysfit = xs.map!((x) => f(x)).array;
    auto ysnoise = xs.map!((x) => f(x) + uniform(-width(x),width(x))).array;
    // Adding colour makes it stop working
    auto aes = Aes!(typeof(xs), "x",
        typeof(ysnoise), "y", string[], "colour" )( xs, ysnoise, ("a").repeat(xs.length).array );
    auto gg = GGPlotD() + geomPoint( aes );
    gg + geomLine( Aes!(typeof(xs), "x",
        typeof(ysfit), "y" )( xs, ysfit ) );

    //  
    auto ys2fit = xs.map!((x) => 1-f(x)).array;
    auto ys2noise = xs.map!((x) => 1-f(x) + uniform(-width(x),width(x))).array;

    gg + geomLine( Aes!(typeof(xs), "x",
        typeof(ysfit), "y" )( xs, ys2fit ) );
    gg + geomPoint( Aes!(typeof(xs), "x",
        typeof(ysnoise), "y", string[], "colour" )( xs, ys2noise, ("b").repeat(xs.length).array ) );

    gg.save( "noise.png" );
}
```

![A histogram](http://blackedder.github.io/ggplotd/images/hist.png)
```D
import ggplotd.ggplotd;

void main()
{
    import std.array : array;
    import std.algorithm : map;
    import std.range : repeat, iota;
    import std.random : uniform;
    auto xs = iota(0,25,1).map!((x) => uniform(0.0,5)+uniform(0.0,5)).array;
    auto aes = Aes!(typeof(xs), "x")( xs );
    auto gg = GGPlotD() + geomHist( aes );

    auto ys = (0.0).repeat( xs.length ).array;
    auto aesPs = aes.merge( Aes!(double[], "y", double[], "colour" )
        ( ys, ys ) );
    gg + geomPoint( aesPs );

    gg.save( "hist.png" );
}
```

## Extending GGplotD

Due to GGplotD’s design it is relatively straightforward to extend GGplotD to
support new types of plots. This is especially true if your function depends
on the already implemented base types geomLine and geomPoint. The main reason
for not having added more functions yet is lack of time. If you decide to
implement your own function then **please** open a pull request or at least
copy your code into an issue. That way we can all benefit from your work :)
Even if you think the code is not up to scratch it will be easier for the
maintainer(s) to take your code and adapt it than to start from scrap.


### geom*

In general a geom* function reads the data, does some transformation on it
and then returns a struct containing the transformed result. In GGPlotD
the low level geom* function such as geomLine and geomPoint draw directly
to a cairo.Context. Luckily most higher level geom* functions can just
rely on calling geomLine and geomPoint. For reference see below for the
geomHist drawing implementation. Again if you decide to define your own
function then please let us know and send us the code. That way we can add
the function to the library and everyone can benefit.

```D 

/// Draw histograms based on the x coordinates of the data (aes)
auto geomHist(AES)(AES aes)
{
    import std.algorithm : map;
    import std.array : array;
    import std.range : repeat;

    // New aes to hold the lines for drawing histogram
    auto aesLine = Aes!(double[], "x", double[], "y", 
            typeof(aes.front.colour)[], "colour" )();

    foreach( grouped; group( aes ) ) // Split data by colour/id
    {
        auto bins = grouped
            .map!( (t) => t.x ) // Extract the x coordinates
            .array.bin( 11 );   // Bin the data
        foreach( bin; bins )
        {
            // Convert data into line coordinates
            aesLine.x ~= [ bin.range[0], bin.range[0],
               bin.range[1], bin.range[1] ];
            aesLine.y ~= [ 0, bin.count,
               bin.count, 0 ];

            // Each (new) line coordinate has the colour specified
            // in the original data
            aesLine.colour ~= grouped.front.colour.repeat(4).array;
        }
    }
    // Use the xs/ys/colours to draw lines
    return geomLine( aesLine );
}

```

Note that the above highlights the drawing part of the function.
Converting the data into bins is done in a separate bin function, which
can be found in the [code](./source/ggplotd/geom.d#L204).

### stat*

ggplot2 for R defines a number of functions that plot statistics of the
data. GGplotD does not come with any such functions out of the box, but
the implementation should be very similar to the above named geom*
functions. The main difference will be that a stat function will have to
do more data analysis. In that way the line between geom and stat
functions is quite blurry; it could be argued that geomHist is a stat
function. If you are interested in adding support for more advanced
statistics then you should use the [geom* example](#geom) as a starting
point. 

## References

Wilkinson, Leland. The Grammar of Graphics. Springer Science & Business Media, 2013.

