gdpviewer
==========

Gdpviewer is a gui tool for diaplaying gstreamer gdp data. This is for debugging gstreamer`s plugins and pipelines.

Usage
-----

1) Grab gdp data via gdppay element: gst-launch-1.0 videotestsrc ! gdppay ! filesink location=dump.gdp

2) Display in gdpviewer application



Gui
-----

![alt tag](https://cloud.githubusercontent.com/assets/10683398/8769317/274bacaa-2ebc-11e5-90c7-d179aef4b501.png)


Building requirements:
-----

* qt (4,5)

* gstreamer-1.0

* pkgconfig


Building:
-----

cd gdpviewer

chmod +x src/verinfo/verinfo.sh

qmake gdpviewer.pro

make gitinfo

make
