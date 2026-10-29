---
id: "notion-3bad0cf70a7e810db545c556a50197db"
title: "Remote Sensing & GIS Glossary — Earth Observation Terms"
type: "reference"
status: "active"
created: "2026-10-16"
updated: "2026-10-16"
owner: "zeus"
tags: ["type/reference", "domain/so", "collection/so", "kind/record"]
collection: "so"
kind: "record"
notion_database: "Geo DB"
notion_status: "Not started"
notion_tags: ["Architecture", "Drones"]
notion_category: "Geo"
notion_id: "3bad0cf70a7e810db545c556a50197db"
notion_url: "https://app.notion.com/p/Remote-Sensing-GIS-Glossary-Earth-Observation-Terms-3bad0cf70a7e810db545c556a50197db"
notion_created: "2026-08-12"
notion_last_edited: "2026-08-15T22:32:00.000Z"
imported: "2026-10-16"
---

# Remote Sensing & GIS Glossary — Earth Observation Terms

> Source: SkyWatch Remote Sensing & GIS Glossary. All terms from the EO/geospatial domain — essential vocabulary for working with satellite imagery, TAK, QGIS, PostGIS, and the OneMind geo stack.
---
# — A —
**Aerial Imagery***  [Sensors & Data Types]*
Imagery captured from crewed aircraft flying below satellite altitude. Typically offers very high spatial resolution over a defined area. Well suited to detailed mapping and inspection of cities, sites, and infrastructure.
**Albedo***  [Analysis & Indices]*
The fraction of incoming sunlight a surface reflects, from 0 (fully absorbing) to 1 (fully reflecting). Snow and ice have high albedo; forests and asphalt have low albedo — useful in climate, energy, and land-cover analysis.
**Analysis-Ready Data (ARD)***  [Processing & Formats]*
Satellite data already corrected, calibrated, and standardized — typically georeferenced, atmospherically corrected, and tiled — so analysts can use it immediately without heavy pre-processing.
**Area of Interest (AOI)***  [Access & Tasking]*
The specific geographic area you want imagery for, usually drawn as a polygon or bounding box. Your AOI defines what gets searched, priced, tasked, and delivered.
**Atmospheric Correction***  [Processing & Formats]*
The process of removing the effects of the atmosphere — haze, scattering, and absorption — from raw imagery, so pixel values reflect ground conditions rather than the air in between. Essential for comparing imagery over time.
---
# — B —
**Band***  [Sensors & Data Types]*
A discrete range of wavelengths a sensor records, such as red, green, blue, or near-infrared. Combining bands in different ways produces natural-color views, false-color composites, and spectral indices.
**Basemap***  [GIS Fundamentals]*
The reference layer of imagery or cartography that other geospatial data is drawn on top of. Provides geographic context — roads, terrain, or recent satellite imagery — without being the primary subject of analysis.
**Bathymetry***  [Analysis & Indices]*
The measurement of water depth and the shape of underwater terrain. Satellite-derived bathymetry uses how light penetrates and reflects through shallow water to map coastlines, reefs, and seabeds.
---
# — C —
**Change Detection***  [Analysis & Indices]*
Comparing imagery of the same location captured at different times to identify what has changed — new construction, deforestation, flooding, or encroachment. One of the highest-value workflows in Earth observation.
**Cloud Cover***  [Resolution & Quality]*
The percentage of an image obscured by clouds. High cloud cover can make optical imagery unusable, which is why analysts filter by it — and why cloud-penetrating radar (SAR) matters in persistently cloudy regions.
**Coordinate Reference System (CRS)***  [GIS Fundamentals]*
The framework that ties coordinates in your data to real positions on Earth, defining the datum, projection, and units. Two datasets must share a CRS — or be reprojected to one — before they can be accurately overlaid.
---
# — D —
**Digital Elevation Model (DEM)***  [Analysis & Indices]*
A 3D representation of terrain elevation stored as a grid of height values. Underpins orthorectification, flood modeling, line-of-sight analysis, and slope and viewshed calculations. See also: DTED, SRTM.
**Digital Surface Model (DSM)***  [Analysis & Indices]*
An elevation model that includes everything on the surface — buildings, vegetation, and structures — in addition to the bare ground. Useful for measuring heights of objects and modeling line of sight.
**Digital Terrain Model (DTM)***  [Analysis & Indices]*
An elevation model of the bare earth with surface features like buildings and trees removed. Used for hydrology, engineering, and terrain analysis where the underlying ground matters.
**Drone Imagery (UAV)***  [Sensors & Data Types]*
Imagery collected by uncrewed aerial vehicles flying at low altitude. Delivers extremely high resolution over small areas on demand — ideal for close inspection, surveying, and progress monitoring of individual sites.
---
# — E —
**Earth Observation (EO)***  [Sensors & Data Types]*
The collection of information about Earth's physical, chemical, and biological systems using remote sensing — primarily satellites, aircraft, and drones. The umbrella term for the entire field.
**Electromagnetic Spectrum***  [Sensors & Data Types]*
The full range of energy wavelengths, from radio waves to gamma rays. Remote sensing instruments record specific portions — visible light, infrared, microwave — and each reveals different things about the surface.
---
# — F —
**False-Color Composite***  [Analysis & Indices]*
An image that maps non-visible bands (such as near-infrared) to visible colors to highlight features the eye can't normally see. A common false-color view renders healthy vegetation in bright red.
**Feature Extraction***  [Analysis & Indices]*
Automatically identifying and outlining objects of interest in imagery — buildings, roads, water bodies, vehicles — to turn raw pixels into structured, mappable data.
**Footprint***  [Access & Tasking]*
The area on the ground covered by a single image or scene. Comparing an image's footprint to your AOI tells you how much of your target area a given capture actually contains.
---
# — G —
**Geographic Information System (GIS)***  [GIS Fundamentals]*
Software for storing, analyzing, visualizing, and managing spatial data. Platforms like ArcGIS and QGIS let teams combine imagery with other layers — parcels, assets, sensors — to answer location-based questions.
**Georeferencing***  [Processing & Formats]*
Assigning real-world coordinates to an image so each pixel maps to a known location on Earth. Without it, imagery can't be measured, overlaid, or compared accurately against other data.
**GeoTIFF***  [Processing & Formats]*
A widely used raster image format that embeds georeferencing information — coordinate system and position — directly in the file, so imagery opens in the right place in any GIS without a separate world file.
**Ground Control Point (GCP)***  [Processing & Formats]*
A location whose precise coordinates are known on the ground and identifiable in an image. GCPs are used to georeference and orthorectify imagery, improving positional accuracy.
**Ground Sample Distance (GSD)***  [Resolution & Quality]*
The real-world distance represented by one pixel in an image — e.g. 30cm GSD means each pixel covers a 30cm square on the ground. Smaller GSD = finer detail. Often used interchangeably with spatial resolution.
---
# — H —
**Hyperspectral Imaging***  [Sensors & Data Types]*
Imaging that captures hundreds of narrow, contiguous spectral bands, producing a detailed spectral fingerprint for every pixel. Can distinguish materials and minerals that look identical in ordinary color imagery.
---
# — I —
**Incidence Angle***  [Resolution & Quality]*
The angle at which a sensor views a target relative to straight down (nadir). Larger off-nadir angles let satellites see to the side and revisit faster, but can distort geometry and stretch ground resolution.
**Interferometry (InSAR)***  [Analysis & Indices]*
A radar technique that compares the phase of two or more SAR images to measure tiny ground movements — millimeters of subsidence, uplift, or structural shift — over time.
---
# — L —
**LiDAR***  [Sensors & Data Types]*
Light Detection and Ranging — an active sensor that fires laser pulses and times their return to build precise 3D point clouds of terrain, structures, and vegetation, even penetrating tree canopy to map the ground beneath.
---
# — M —
**Metadata***  [Processing & Formats]*
Descriptive information packaged with an image: when and where it was captured, by which sensor, at what resolution, cloud cover, sun angle. Metadata is how you find, filter, and trust imagery.
**Mosaic***  [Processing & Formats]*
A single seamless image assembled by stitching together multiple overlapping scenes. Mosaics provide continuous coverage of an area too large for one capture.
**Multispectral Imaging***  [Sensors & Data Types]*
Imaging that records a handful of broad spectral bands — typically red, green, blue, and near-infrared. Supports natural-color views plus vegetation, water, and land-cover analysis. The workhorse of optical EO.
---
# — N —
**Nadir***  [Orbits & Platforms]*
The point on the ground directly beneath a sensor. A nadir view looks straight down, giving the truest geometry and least distortion; off-nadir views look to the side.
**NDVI***  [Analysis & Indices]*
Normalized Difference Vegetation Index — a ratio of near-infrared to red reflectance that measures vegetation health and density. Values near 1 = dense healthy vegetation; low values = bare soil, water, or stress.
---
# — O —
**Optical Imagery***  [Sensors & Data Types]*
Imagery formed from reflected sunlight in the visible and infrared bands — the familiar photographic satellite view. Information-rich but depends on daylight and clear skies. Contrast with SAR.
**Orbit***  [Orbits & Platforms]*
The path a satellite follows around Earth. Earth-observation satellites usually fly in Low Earth Orbit (LEO) for detail, with the specific orbit determining coverage, revisit frequency, and lighting conditions.
**Orthorectification***  [Processing & Formats]*
Correcting an image for terrain relief, sensor tilt, and viewing geometry so every pixel sits in its true map position. Orthorectified imagery can be measured and overlaid like a map.
---
# — P —
**Panchromatic***  [Sensors & Data Types]*
A single-band image recording overall brightness across a wide visible range, usually at higher spatial resolution than color bands. Often fused with color bands via pan-sharpening.
**Pan-sharpening***  [Processing & Formats]*
Merging a high-resolution panchromatic band with lower-resolution color bands to produce a single image that is both sharp and in color — combining the best of each.
**Pixel***  [Resolution & Quality]*
The smallest element of a digital image. Each pixel represents a measured patch of ground; how much ground it covers is the image's ground sample distance (GSD).
**Point Cloud***  [Processing & Formats]*
A set of 3D points, each with a position and often color or intensity, describing the shape of surfaces and objects. Produced by LiDAR and by photogrammetry from overlapping imagery.
---
# — R —
**Radiometric Resolution***  [Resolution & Quality]*
How finely a sensor distinguishes differences in energy or brightness, expressed in bits. Higher radiometric resolution (e.g. 12-bit vs 8-bit) captures subtler tonal variation in shadows and highlights.
**Raster***  [GIS Fundamentals]*
Data stored as a grid of cells or pixels, each holding a value — the structure of all imagery and continuous surfaces like elevation. Contrast with vector data (points, lines, polygons).
**Remote Sensing***  [Sensors & Data Types]*
Gathering information about an object or area from a distance — without physical contact — typically by measuring reflected or emitted energy from satellites, aircraft, or drones. The core method behind Earth observation.
**Resampling***  [Processing & Formats]*
Recalculating pixel values when an image is reprojected, resized, or aligned to a new grid. Method chosen (nearest-neighbor, bilinear, cubic) affects sharpness and accuracy of the result.
**Revisit Time***  [Orbits & Platforms]*
How often a satellite or constellation can image the same location. Shorter revisit times = fresher data and finer change detection — daily or multiple times per day with large constellations.
---
# — S —
**Spatial Resolution***  [Resolution & Quality]*
The level of ground detail an image captures, defined by the size of the smallest distinguishable feature — closely tied to GSD. Higher spatial resolution reveals smaller objects.
**Spectral Resolution***  [Resolution & Quality]*
The number and narrowness of spectral bands a sensor records. Higher spectral resolution — as in hyperspectral imaging — lets you distinguish materials with similar appearance but different spectral signatures.
**Spectral Signature***  [Analysis & Indices]*
The characteristic pattern of how a material reflects and absorbs energy across wavelengths. Water, vegetation, soil, and minerals each have distinct signatures, allowing spectral data to identify what's on the ground.
**Sun-Synchronous Orbit (SSO)***  [Orbits & Platforms]*
A near-polar orbit timed so the satellite passes over each location at roughly the same local sun time on every revisit. Consistent lighting makes images far easier to compare across days and seasons.
**Supervised Classification***  [Analysis & Indices]*
Sorting image pixels into land-cover classes using training samples an analyst has labeled. The algorithm learns from the examples, then assigns every pixel to the most similar class.
**Swath Width***  [Orbits & Platforms]*
The width of the strip of ground a sensor images in a single pass. Wider swaths cover more area per orbit, supporting broad monitoring; narrow swaths often trade coverage for higher resolution.
**Synthetic Aperture Radar (SAR)***  [Sensors & Data Types]*
An active radar sensor that emits microwave pulses and measures their echo, building imagery independent of sunlight and able to see through clouds, smoke, and darkness — invaluable for all-weather, day-or-night monitoring.
---
# — T —
**Tasking***  [Access & Tasking]*
Commissioning a satellite to capture new imagery of a specific location at a future time, rather than buying from the existing archive. Tasking gets you fresh data of exactly the area and window you need.
**Temporal Resolution***  [Resolution & Quality]*
How frequently imagery of a location is available over time — essentially the cadence of revisit. High temporal resolution is what makes monitoring and timely change detection possible.
**Thermal Infrared***  [Sensors & Data Types]*
The portion of the infrared spectrum tied to heat emitted by surfaces. Thermal imagery reveals temperature differences — useful for detecting fires, equipment activity, water stress, and urban heat islands.
**True-Color Composite***  [Analysis & Indices]*
An image combining the red, green, and blue bands to show a scene roughly as the human eye would see it from above. The most intuitive way to view optical imagery.
---
# — U —
**Unsupervised Classification***  [Analysis & Indices]*
Grouping image pixels into clusters by statistical similarity without pre-labeled training data. The analyst interprets and labels the resulting clusters afterward — useful when ground truth is scarce.
---
# — V —
**Vector***  [GIS Fundamentals]*
Spatial data represented as discrete geometry — points, lines, and polygons — with attached attributes, such as building outlines or road networks. The complement to raster data in a GIS.
---
# — W —
**Web Map Service (WMS / WMTS)***  [Access & Tasking]*
Open standards for streaming map imagery over the web as rendered tiles or images. Pull live layers straight into a GIS or web app without downloading and hosting the full dataset.
---
> Total: 40 terms across 7 categories — Sensors & Data Types · Resolution & Quality · Orbits & Platforms · Processing & Formats · Analysis & Indices · GIS Fundamentals · Access & Tasking
---
# 📚 Related Pages
- SkyWatch — Satellite Imagery & Geospatial Data Platform
- Elevation Data — DTED, SRTM, USGS & TAK Integration
- PostGIS — Geospatial Extension for PostgreSQL
- TAK Server & Client Ecosystem — Reference Guide
