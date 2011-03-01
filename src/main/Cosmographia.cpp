// This file is part of Cosmographia.
//
// Copyright (C) 2010 Chris Laurel <claurel@gmail.com>
//
// Cosmographia is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// Cosmographia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with Cosmographia. If not, see <http://www.gnu.org/licenses/>.

#include <QtGui>

#include "UniverseView.h"
#include "UniverseCatalog.h"
#include "UniverseLoader.h"
#include "Cosmographia.h"
#if FFMPEG_SUPPORT
#include "QVideoEncoder.h"
#endif
#include "JPLEphemeris.h"
#include "NetworkTextureLoader.h"
#include "LinearCombinationTrajectory.h"
#include "compatibility/CatalogParser.h"
#include "compatibility/TransformCatalog.h"
#include "astro/IAULunarRotationModel.h"
#include <vesta/GregorianDate.h>
#include <vesta/Body.h>
#include <vesta/Arc.h>
#include <vesta/Trajectory.h>
#include <vesta/Frame.h>
#include <vesta/InertialFrame.h>
#include <vesta/RotationModel.h>
#include <vesta/KeplerianTrajectory.h>
#include <vesta/FixedPointTrajectory.h>
#include <vesta/WorldGeometry.h>
#include <vesta/Units.h>
#include <qjson/parser.h>
#include <qjson/serializer.h>
#include <algorithm>
#include <QDateTimeEdit>
#include <QBoxLayout>
#include <QDialogButtonBox>

#include <QNetworkDiskCache>
#include <QNetworkRequest>

using namespace vesta;
using namespace Eigen;


Cosmographia::Cosmographia() :
    m_catalog(NULL),
    m_view3d(NULL),
    m_loader(NULL),
    m_fullScreenAction(NULL),
    m_networkManager(NULL)
{
    m_catalog = new UniverseCatalog();
    m_view3d = new UniverseView();
    m_loader = new UniverseLoader();

    setCentralWidget(m_view3d);

    setWindowTitle(tr("Cosmographia"));

    /*** File Menu ***/
    QMenu* fileMenu = new QMenu("&File", this);
    QAction* saveScreenShotAction = fileMenu->addAction("&Save Screen Shot");
    QAction* recordVideoAction = fileMenu->addAction("&Record Video");
    recordVideoAction->setShortcut(QKeySequence("Ctrl+R"));
#if !FFMPEG_SUPPORT
    recordVideoAction->setEnabled(false);
#endif
    fileMenu->addSeparator();
    QAction* loadCatalogAction = fileMenu->addAction("&Load Catalog...");
    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction("&Quit");
    this->menuBar()->addMenu(fileMenu);

    connect(saveScreenShotAction, SIGNAL(triggered()), this, SLOT(saveScreenShot()));
    connect(recordVideoAction, SIGNAL(triggered()), this, SLOT(recordVideo()));
    connect(loadCatalogAction, SIGNAL(triggered()), this, SLOT(loadCatalog()));
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));

    /*** Time Menu ***/
    QMenu* timeMenu = new QMenu("&Time", this);
    QAction* setTimeAction = new QAction("Set &Time...", timeMenu);
    setTimeAction->setShortcut(QKeySequence("Ctrl+T"));
    timeMenu->addAction(setTimeAction);
    timeMenu->addSeparator();
    QAction* pauseAction = new QAction("&Pause", timeMenu);
    pauseAction->setCheckable(true);
    pauseAction->setShortcut(Qt::Key_Space);
    timeMenu->addAction(pauseAction);
    QAction* fasterAction = new QAction("&Faster", timeMenu);
    fasterAction->setShortcut(QKeySequence("Ctrl+L"));
    timeMenu->addAction(fasterAction);
    QAction* slowerAction = new QAction("&Slower", timeMenu);
    slowerAction->setShortcut(QKeySequence("Ctrl+K"));
    timeMenu->addAction(slowerAction);
    QAction* faster2Action = new QAction("2x Faster", timeMenu);
    faster2Action->setShortcut(QKeySequence("Ctrl+Shift+L"));
    timeMenu->addAction(faster2Action);
    QAction* slower2Action = new QAction("2x Slower", timeMenu);
    slower2Action->setShortcut(QKeySequence("Ctrl+Shift+K"));
    timeMenu->addAction(slower2Action);
    QAction* backYearAction = new QAction("Back one year", timeMenu);
    backYearAction->setShortcut(QKeySequence("Ctrl+["));
    timeMenu->addAction(backYearAction);
    QAction* forwardYearAction = new QAction("Forward one year", timeMenu);
    forwardYearAction->setShortcut(QKeySequence("Ctrl+]"));
    timeMenu->addAction(forwardYearAction);
    QAction* reverseAction = new QAction("&Reverse", timeMenu);
    reverseAction->setShortcut(QKeySequence("Ctrl+J"));
    timeMenu->addAction(reverseAction);
    QAction* nowAction = new QAction("&Current time", timeMenu);
    timeMenu->addAction(nowAction);
    this->menuBar()->addMenu(timeMenu);

    connect(setTimeAction, SIGNAL(triggered()),     this,     SLOT(setTime()));
    connect(pauseAction,   SIGNAL(triggered(bool)), m_view3d, SLOT(setPaused(bool)));
    connect(fasterAction,  SIGNAL(triggered()),     this,     SLOT(faster()));
    connect(slowerAction,  SIGNAL(triggered()),     this,     SLOT(slower()));
    connect(faster2Action,  SIGNAL(triggered()),     this,     SLOT(faster2()));
    connect(slower2Action,  SIGNAL(triggered()),     this,     SLOT(slower2()));
    connect(backYearAction,  SIGNAL(triggered()),     this,     SLOT(backYear()));
    connect(forwardYearAction,  SIGNAL(triggered()),     this,     SLOT(forwardYear()));
    connect(reverseAction, SIGNAL(triggered()),     this,     SLOT(reverseTime()));
    connect(nowAction,     SIGNAL(triggered()),     m_view3d, SLOT(setCurrentTime()));

    /*** Camera Menu ***/
    QMenu* cameraMenu = new QMenu("&Camera", this);
    QActionGroup* cameraFrameGroup = new QActionGroup(cameraMenu);
    QAction* inertialAction = new QAction("&Inertial Frame", cameraFrameGroup);
    inertialAction->setShortcut(QKeySequence("Ctrl+I"));
    inertialAction->setCheckable(true);
    inertialAction->setChecked(true);
    cameraMenu->addAction(inertialAction);
    QAction* bodyFixedAction = new QAction("&Body Fixed Frame", cameraFrameGroup);
    bodyFixedAction->setShortcut(QKeySequence("Ctrl+B"));
    bodyFixedAction->setCheckable(true);
    cameraMenu->addAction(bodyFixedAction);
    QAction* synodicAction = new QAction("&Synodic Frame", cameraFrameGroup);
    synodicAction->setShortcut(QKeySequence("Ctrl+Y"));
    synodicAction->setCheckable(true);
    cameraMenu->addAction(synodicAction);
    QAction* centerAction = new QAction("Set &Center", cameraMenu);
    centerAction->setShortcut(QKeySequence("Ctrl+C"));
    cameraMenu->addAction(centerAction);
    QAction* gotoAction = new QAction("&Goto Selected Object", cameraMenu);
    gotoAction->setShortcut(QKeySequence("Ctrl+G"));
    gotoAction->setDisabled(true); // NOT YET IMPLEMENTED
    cameraMenu->addAction(gotoAction);

    this->menuBar()->addMenu(cameraMenu);

    connect(inertialAction,  SIGNAL(triggered(bool)), m_view3d, SLOT(inertialObserver(bool)));
    connect(bodyFixedAction, SIGNAL(triggered(bool)), m_view3d, SLOT(bodyFixedObserver(bool)));
    connect(synodicAction,   SIGNAL(triggered(bool)), m_view3d, SLOT(synodicObserver(bool)));
    connect(centerAction,    SIGNAL(triggered()),     m_view3d, SLOT(setObserverCenter()));
    connect(gotoAction,      SIGNAL(triggered()),     m_view3d, SLOT(gotoSelectedObject()));

    /*** Visual aids menu ***/
    QMenu* visualAidsMenu = new QMenu("&Visual Aids", this);

    QAction* eqGridAction = new QAction("E&quatorial grid", visualAidsMenu);
    eqGridAction->setCheckable(true);
    visualAidsMenu->addAction(eqGridAction);
    QAction* eclipticAction = new QAction("&Ecliptic", visualAidsMenu);
    eclipticAction->setCheckable(true);
    visualAidsMenu->addAction(eclipticAction);
    visualAidsMenu->addSeparator();

    QAction* trajectoriesAction = new QAction("&Trajectories", visualAidsMenu);
    trajectoriesAction->setCheckable(true);
    visualAidsMenu->addAction(trajectoriesAction);
    QAction* planetOrbitsAction = new QAction("Planet &orbits", visualAidsMenu);
    planetOrbitsAction->setShortcut(QKeySequence("Ctrl+O"));
    planetOrbitsAction->setCheckable(true);
    visualAidsMenu->addAction(planetOrbitsAction);
    QAction* plotTrajectoryAction = new QAction("&Plot trajectory", visualAidsMenu);
    plotTrajectoryAction->setShortcut(QKeySequence("Ctrl+P"));
    visualAidsMenu->addAction(plotTrajectoryAction);
    QAction* plotTrajectoryObserverAction = new QAction("&Plot trajectory in observer frame", visualAidsMenu);
    plotTrajectoryObserverAction->setShortcut(QKeySequence("Shift+Ctrl+P"));
    visualAidsMenu->addAction(plotTrajectoryObserverAction);

    visualAidsMenu->addSeparator();
    QAction* infoTextAction = new QAction("Info text", visualAidsMenu);
    infoTextAction->setCheckable(true);
    infoTextAction->setChecked(true);
    visualAidsMenu->addAction(infoTextAction);

    this->menuBar()->addMenu(visualAidsMenu);

    connect(eqGridAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setEquatorialGridVisibility(bool)));
    connect(eclipticAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setEclipticVisibility(bool)));
    connect(trajectoriesAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setTrajectoryVisibility(bool)));
    connect(planetOrbitsAction, SIGNAL(triggered(bool)), this, SLOT(setPlanetOrbitsVisibility(bool)));
    connect(plotTrajectoryAction, SIGNAL(triggered()), this, SLOT(plotTrajectory()));
    connect(plotTrajectoryObserverAction, SIGNAL(triggered()), this, SLOT(plotTrajectoryObserver()));
    connect(infoTextAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setInfoText(bool)));

    /*** Graphics menu ***/
    QMenu* graphicsMenu = new QMenu("&Graphics", this);
    QAction* normalMapAction = new QAction("&Normal map", graphicsMenu);
    normalMapAction->setCheckable(true);
    graphicsMenu->addAction(normalMapAction);
    QAction* shadowsAction = new QAction("&Shadows", graphicsMenu);
    shadowsAction->setCheckable(true);
    graphicsMenu->addAction(shadowsAction);
    QAction* atmospheresAction = new QAction("&Atmosphere", graphicsMenu);
    atmospheresAction->setCheckable(true);
    atmospheresAction->setShortcut(QKeySequence("Ctrl+A"));
    graphicsMenu->addAction(atmospheresAction);
    QAction* cloudLayerAction = new QAction("&Cloud layer", graphicsMenu);
    cloudLayerAction->setCheckable(true);
    graphicsMenu->addAction(cloudLayerAction);
    QAction* realisticPlanetsAction = new QAction("Realistic &planets", graphicsMenu);
    realisticPlanetsAction->setCheckable(true);
    graphicsMenu->addAction(realisticPlanetsAction);
    QAction* ambientLightAction = new QAction("Ambient &light", graphicsMenu);
    ambientLightAction->setCheckable(true);
    ambientLightAction->setChecked(true);
    graphicsMenu->addAction(ambientLightAction);
    QAction* reflectionsAction = new QAction("&Reflections", graphicsMenu);
    reflectionsAction->setCheckable(true);
    graphicsMenu->addAction(reflectionsAction);
    QAction* milkyWayAction = new QAction("&Milky Way", graphicsMenu);
    milkyWayAction->setCheckable(true);
    milkyWayAction->setShortcut(QKeySequence("Ctrl+M"));
    graphicsMenu->addAction(milkyWayAction);
    QAction* asteroidsAction = new QAction("As&teroids", graphicsMenu);
    asteroidsAction->setCheckable(true);
    asteroidsAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
    graphicsMenu->addAction(asteroidsAction);
    QAction* highlightAsteroidsAction = new QAction("Highlight asteroid family", graphicsMenu);
    highlightAsteroidsAction->setShortcut(QKeySequence("Ctrl+Alt+T"));
    graphicsMenu->addAction(highlightAsteroidsAction);
    graphicsMenu->addSeparator();
    m_fullScreenAction = new QAction("Full Screen", graphicsMenu);
    m_fullScreenAction->setShortcut(QKeySequence("Ctrl+F"));
    m_fullScreenAction->setCheckable(true);
    graphicsMenu->addAction(m_fullScreenAction);
    connect(m_fullScreenAction, SIGNAL(toggled(bool)), this, SLOT(setFullScreen(bool)));
    QAction* anaglyphAction = new QAction("Anaglyph stereo", graphicsMenu);
    anaglyphAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
    anaglyphAction->setCheckable(true);
    graphicsMenu->addAction(anaglyphAction);

    this->menuBar()->addMenu(graphicsMenu);

    connect(normalMapAction,        SIGNAL(triggered(bool)), m_view3d, SLOT(setNormalMaps(bool)));
    connect(shadowsAction,          SIGNAL(triggered(bool)), m_view3d, SLOT(setShadows(bool)));
    connect(atmospheresAction,      SIGNAL(triggered(bool)), m_view3d, SLOT(setAtmospheres(bool)));
    //connect(cloudLayerAction,       SIGNAL(triggered(bool)), m_view3d, SLOT(setCloudLayerVisibility(bool)));
    connect(realisticPlanetsAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setRealisticPlanets(bool)));
    connect(ambientLightAction,     SIGNAL(triggered(bool)), m_view3d, SLOT(setAmbientLight(bool)));
    connect(reflectionsAction,      SIGNAL(triggered(bool)), m_view3d, SLOT(setReflections(bool)));
    connect(milkyWayAction,         SIGNAL(triggered(bool)), m_view3d, SLOT(setMilkyWayVisibility(bool)));
    connect(asteroidsAction,        SIGNAL(triggered(bool)), m_view3d, SLOT(setAsteroidVisibility(bool)));
    connect(highlightAsteroidsAction, SIGNAL(triggered()),   m_view3d, SLOT(highlightAsteroidFamily()));
    connect(anaglyphAction,         SIGNAL(triggered(bool)), m_view3d, SLOT(setAnaglyphStereo(bool)));

    /*** Help menu ***/
    QMenu* helpMenu = new QMenu("Help", this);

    QAction* aboutAction = new QAction("About QtCosmographia", helpMenu);
    helpMenu->addAction(aboutAction);
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

    this->menuBar()->addMenu(helpMenu);

    setCursor(QCursor(Qt::CrossCursor));

    loadSettings();

    Matrix3d m = InertialFrame::equatorJ2000()->orientation().toRotationMatrix();
    std::cout << m.format(8) << std::endl;

    Vector3d pole = m * Vector3d::UnitZ();
    double obl = acos(pole.z());
    Vector3d equinox = m * Vector3d::UnitY();
    double eq = acos(equinox.y());
    std::cout << "pole: " << radiansToArcsec(obl) << ", equinox: " << radiansToArcsec(eq) << std::endl;
}


Cosmographia::~Cosmographia()
{
    saveSettings();
}


// Convert a JPL ephemeris orbit from SSB-centered to Sun-centered
static Trajectory*
createSunRelativeTrajectory(const JPLEphemeris* eph, JPLEphemeris::JplObjectId id)
{
    LinearCombinationTrajectory* orbit = new LinearCombinationTrajectory(eph->trajectory(id), 1.0,
                                                                         eph->trajectory(JPLEphemeris::Sun), -1.0);
    orbit->setPeriod(eph->trajectory(id)->period());
    return orbit;
}


/** Perform once-per-run initialization, such as loading planetary ephemerides.
  */
void
Cosmographia::initialize()
{
    // Set up builtin orbits
    JPLEphemeris* eph = JPLEphemeris::load("de406_1800-2100.dat");
    if (eph)
    {
        m_loader->addBuiltinOrbit("Sun",     eph->trajectory(JPLEphemeris::Sun));
        m_loader->addBuiltinOrbit("Moon",    eph->trajectory(JPLEphemeris::Moon));

        // The code below will create planet trajectories relative to the SSB
        /*
        m_loader->addBuiltinOrbit("Mercury", eph->trajectory(JPLEphemeris::Mercury));
        m_loader->addBuiltinOrbit("Venus",   eph->trajectory(JPLEphemeris::Venus));
        m_loader->addBuiltinOrbit("EMB",     eph->trajectory(JPLEphemeris::EarthMoonBarycenter));
        m_loader->addBuiltinOrbit("Mars",    eph->trajectory(JPLEphemeris::Mars));
        m_loader->addBuiltinOrbit("Jupiter", eph->trajectory(JPLEphemeris::Jupiter));
        m_loader->addBuiltinOrbit("Saturn",  eph->trajectory(JPLEphemeris::Saturn));
        m_loader->addBuiltinOrbit("Uranus",  eph->trajectory(JPLEphemeris::Uranus));
        m_loader->addBuiltinOrbit("Neptune", eph->trajectory(JPLEphemeris::Neptune));
        m_loader->addBuiltinOrbit("Pluto",   eph->trajectory(JPLEphemeris::Pluto));
        */

        Trajectory* embTrajectory = createSunRelativeTrajectory(eph, JPLEphemeris::EarthMoonBarycenter);
        m_loader->addBuiltinOrbit("EMB", embTrajectory);

        m_loader->addBuiltinOrbit("Mercury", createSunRelativeTrajectory(eph, JPLEphemeris::Mercury));
        m_loader->addBuiltinOrbit("Venus",   createSunRelativeTrajectory(eph, JPLEphemeris::Venus));
        m_loader->addBuiltinOrbit("Mars",    createSunRelativeTrajectory(eph, JPLEphemeris::Mars));
        m_loader->addBuiltinOrbit("Jupiter", createSunRelativeTrajectory(eph, JPLEphemeris::Jupiter));
        m_loader->addBuiltinOrbit("Saturn",  createSunRelativeTrajectory(eph, JPLEphemeris::Saturn));
        m_loader->addBuiltinOrbit("Uranus",  createSunRelativeTrajectory(eph, JPLEphemeris::Uranus));
        m_loader->addBuiltinOrbit("Neptune", createSunRelativeTrajectory(eph, JPLEphemeris::Neptune));
        m_loader->addBuiltinOrbit("Pluto",   createSunRelativeTrajectory(eph, JPLEphemeris::Pluto));

        // m = the ratio of the Moon's to the mass of the Earth-Moon system
        double m = 1.0 / (1.0 + eph->earthMoonMassRatio());
        LinearCombinationTrajectory* earthTrajectory =
                new LinearCombinationTrajectory(embTrajectory, 1.0,
                                                eph->trajectory(JPLEphemeris::Moon), -m);
        earthTrajectory->setPeriod(embTrajectory->period());
        m_loader->addBuiltinOrbit("Earth", earthTrajectory);

        // JPL HORIZONS results for position of Moon with respect to Earth at 1 Jan 2000 12:00
        // position: -2.916083884571964E+05 -2.667168292374240E+05 -7.610248132320160E+04
        // velocity:  6.435313736079528E-01 -6.660876955662288E-01 -3.013257066079174E-01
        //std::cout << "Moon @ J2000:  " << eph->trajectory(JPLEphemeris::Moon)->position(0.0).transpose().format(16) << std::endl;

        // JPL HORIZONS results for position of Earth with respect to Sun at 1 Jan 2000 12:00
        // position: -2.649903422886233E+07  1.327574176646856E+08  5.755671744790662E+07
        // velocity: -2.979426004836674E+01 -5.018052460415045E+00 -2.175393728607054E+00
        //std::cout << "Earth @ J2000: " << earthTrajectory->position(0.0).transpose().format(16) << std::endl;
    }

    // Set up builtin rotation models
    m_loader->addBuiltinRotationModel("IAU Moon", new IAULunarRotationModel());

    // Set up the network manager
    m_networkManager = new QNetworkAccessManager();
    QNetworkDiskCache* cache = new QNetworkDiskCache();
    cache->setCacheDirectory(QDesktopServices::storageLocation(QDesktopServices::CacheLocation));
    m_networkManager->setCache(cache);
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(processReceivedResource(QNetworkReply*)));

    // Set up the texture loader
    m_loader->setTextureLoader(m_view3d->textureLoader());

    loadCatalogFile("solarsys.json");
}


static QDateTime
vestaDateToQtDate(const GregorianDate& date)
{
    return QDateTime(QDate(date.year(), date.month(), date.day()),
                     QTime(date.hour(), date.minute(), date.second(), date.usec() / 1000),
                     Qt::UTC);
}


static GregorianDate
qtDateToVestaDate(const QDateTime& d)
{
    return GregorianDate(d.date().year(), d.date().month(), d.date().day(),
                         d.time().hour(), d.time().minute(), d.time().second(), d.time().msec() * 1000,
                         TimeScale_TDB);
}


void
Cosmographia::setTime()
{
    QDialog timeDialog;
    timeDialog.setWindowTitle("Set Time and Date");
    QDateTimeEdit* timeEdit = new QDateTimeEdit(&timeDialog);

    QVBoxLayout* vbox = new QVBoxLayout(&timeDialog);
    timeDialog.setLayout(vbox);

    QHBoxLayout* hbox = new QHBoxLayout();
    hbox->addWidget(new QLabel("Enter date: ", &timeDialog));
    hbox->addWidget(timeEdit);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &timeDialog);
    vbox->addItem(hbox);
    vbox->addWidget(buttons);

    connect(buttons, SIGNAL(accepted()), &timeDialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &timeDialog, SLOT(reject()));

    double tsec = m_view3d->simulationTime();
    GregorianDate simDate = GregorianDate::TDBDateFromTDBSec(tsec);
    timeEdit->setDateTime(vestaDateToQtDate(simDate));

    if (timeDialog.exec() == QDialog::Accepted)
    {
        QDateTime newDate = timeEdit->dateTime();
        m_view3d->setSimulationTime(qtDateToVestaDate(newDate).toTDBSec());
    }
}


void
Cosmographia::faster()
{
    double t = m_view3d->timeScale() * 10.0;
    if (t < -1.0e7)
        t = -1.0e7;
    else if (t > 1.0e7)
        t = 1.0e7;
    m_view3d->setTimeScale(t);
}


void
Cosmographia::slower()
{
    double t = m_view3d->timeScale() * 0.1;
    if (t > 0.0)
        t = std::max(1.0e-3, t);
    else if (t < 0.0)
        t = std::min(-1.0e-3, t);
    m_view3d->setTimeScale(t);
}


void
Cosmographia::faster2()
{
    double t = m_view3d->timeScale() * 2.0;
    if (t < -1.0e7)
        t = -1.0e7;
    else if (t > 1.0e7)
        t = 1.0e7;
    m_view3d->setTimeScale(t);
}


void
Cosmographia::slower2()
{
    double t = m_view3d->timeScale() * 0.5;
    if (t > 0.0)
        t = std::max(1.0e-3, t);
    else if (t < 0.0)
        t = std::min(-1.0e-3, t);
    m_view3d->setTimeScale(t);
}


void
Cosmographia::backYear()
{
    vesta::GregorianDate d = vesta::GregorianDate::UTCDateFromTDBSec(m_view3d->simulationTime());
    m_view3d->setSimulationTime(vesta::GregorianDate(d.year() - 1, d.month(), d.day(), d.hour(), d.minute(), d.second()).toTDBSec());
}


void
Cosmographia::forwardYear()
{
    vesta::GregorianDate d = vesta::GregorianDate::UTCDateFromTDBSec(m_view3d->simulationTime());
    m_view3d->setSimulationTime(vesta::GregorianDate(d.year() + 1, d.month(), d.day(), d.hour(), d.minute(), d.second()).toTDBSec());
}


void
Cosmographia::reverseTime()
{
    m_view3d->setTimeScale(-m_view3d->timeScale());
}


void
Cosmographia::plotTrajectory()
{
    Entity* body = m_view3d->selectedBody();
    if (body)
    {
        QString name = QString::fromUtf8(body->name().c_str());
        BodyInfo* info = m_catalog->findInfo(name);

        m_view3d->plotTrajectory(body, info);
    }
}


void
Cosmographia::plotTrajectoryObserver()
{
    Entity* body = m_view3d->selectedBody();
    if (body)
    {
        QString name = QString::fromUtf8(body->name().c_str());
        BodyInfo* info = m_catalog->findInfo(name);

        m_view3d->plotTrajectoryObserver(info);
    }
}


void
Cosmographia::setPlanetOrbitsVisibility(bool enabled)
{
    const char* planetNames[] = {
        "Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune", "Moon"
    };

    for (unsigned int i = 0; i < sizeof(planetNames) / sizeof(planetNames[0]); ++i)
    {
        Entity* planet = m_catalog->find(planetNames[i]);
        BodyInfo* info = m_catalog->findInfo(planetNames[i]);

        m_view3d->plotTrajectory(planet, info);
    }
}


void
Cosmographia::setFullScreen(bool enabled)
{
    if (enabled)
    {
        showFullScreen();
    }
    else
    {
        showNormal();
    }
}

void
Cosmographia::about()
{
    QMessageBox::about(this,
                       "Cosmographia",
                       "Cosmographia: "
                       "A celebration of solar system exploration.");
}


void
Cosmographia::saveScreenShot()
{
    QImage screenShot = m_view3d->grabFrameBuffer(false);

    QString defaultFileName = QDesktopServices::storageLocation(QDesktopServices::PicturesLocation) + "/image.png";
    QString saveFileName = QFileDialog::getSaveFileName(this, "Save Image As...", defaultFileName, "*.png *.jpg *.webm *.mov *.ogg");
    if (!saveFileName.isEmpty())
    {
        screenShot.save(saveFileName);
    }
}


void
Cosmographia::loadSettings()
{
    QSettings settings;

    settings.beginGroup("ui");
    m_fullScreenAction->setChecked(settings.value("fullscreen", true).toBool());
    setFullScreen(m_fullScreenAction->isChecked());
    settings.endGroup();
}


void
Cosmographia::saveSettings()
{
    QSettings settings;

    settings.beginGroup("ui");
    settings.setValue("fullscreen", m_fullScreenAction->isChecked());
    settings.endGroup();
}


void
Cosmographia::recordVideo()
{
#if FFMPEG_SUPPORT
    if (m_view3d->isRecordingVideo())
    {
        m_view3d->videoEncoder()->close();
        m_view3d->finishVideoRecording();
    }
    else
    {
        QString defaultFileName = QDesktopServices::storageLocation(QDesktopServices::PicturesLocation) + "/cosmo.mpeg";
        QString saveFileName = QFileDialog::getSaveFileName(this, "Save Video As...", defaultFileName, "Video (*.mkv *.mpeg *.avi)");
        if (!saveFileName.isEmpty())
        {
            QVideoEncoder* encoder = new QVideoEncoder();
            encoder->createFile(saveFileName, 848, 480, 5000000, 20);
            m_view3d->startVideoRecording(encoder);
        }
    }
#endif
}


void
Cosmographia::loadCatalog()
{
    QSettings settings;
    QString defaultFileName = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation) + "/cosmo.json";
    defaultFileName = settings.value("SolarSystemDir", defaultFileName).toString();

    QString solarSystemFileName = QFileDialog::getOpenFileName(this, "Load Catalog", defaultFileName, "Catalog Files (*.json *.ssc)");
    if (!solarSystemFileName.isEmpty())
    {
        loadCatalogFile(solarSystemFileName);
        settings.setValue("SolarSystemDir", solarSystemFileName);
    }
}


void
Cosmographia::loadCatalogFile(const QString& fileName)
{
    if (fileName.isEmpty())
    {
        return;
    }

    QFile solarSystemFile(fileName);
    QString path = QFileInfo(solarSystemFile).absolutePath();

    m_loader->clearResourceRequests();

    if (!solarSystemFile.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, tr("Solar System File Error"), tr("Could not open file '%1'.").arg(fileName));
        return;
    }

    m_loader->setDataSearchPath(path);
    m_loader->setTextureSearchPath(path);
    m_loader->setModelSearchPath(path);

    NetworkTextureLoader* textureLoader = dynamic_cast<NetworkTextureLoader*>(m_loader->textureLoader());
    if (textureLoader)
    {
        textureLoader->setLocalSearchPatch(path);
    }

    if (fileName.toLower().endsWith(".json"))
    {
        QJson::Parser parser;

        bool parseOk = false;
        QVariant result = parser.parse(&solarSystemFile, &parseOk);
        if (!parseOk)
        {
            QMessageBox::warning(this,
                                 tr("Solar System File Error"),
                                 QString("Line %1: %2").arg(parser.errorLine()).arg(parser.errorString()));
            return;
        }

        QVariantMap contents = result.toMap();
        if (contents.empty())
        {
            qDebug() << "Solar system file is empty.";
            return;
        }

        QStringList bodyNames = m_loader->loadSolarSystem(contents, m_catalog);
        foreach (QString name, bodyNames)
        {
            Entity* e = m_catalog->find(name);
            if (e)
            {
                m_view3d->replaceEntity(e, m_catalog->findInfo(name));
            }
        }
    }
    else if (fileName.toLower().endsWith(".ssc"))
    {
        // SSC files expect media and trajectory data files in subdirectories:
        //   trajectories and rotation models - ./data
        //   textures - ./textures/medres
        //   mesh files - ./models
        // Where '.' is the directory containing the ssc file
        m_loader->setDataSearchPath(path + "/data");
        m_loader->setTextureSearchPath(path + "/textures/medres");
        m_loader->setModelSearchPath(path + "/models");
        if (textureLoader)
        {
            textureLoader->setLocalSearchPatch(path + "/textures/medres");
        }

        QVariantList items;

        CatalogParser parser(&solarSystemFile);
        QVariant obj = parser.nextSscObject();
        while (obj.type() == QVariant::Map)
        {
            QJson::Serializer serializer;
            qDebug() << serializer.serialize(obj);

            QVariantMap map = obj.toMap();
            TransformSscObject(&map);
            qDebug() << "Converted: " << serializer.serialize(map);

            QString fullName = map.value("_parent").toString() + "/" + map.value("name").toString();
            map.insert("name", fullName);
            items << map;

            obj = parser.nextSscObject();
        }

        QVariantMap contents;
        contents.insert("name", fileName);
        contents.insert("items", items);

        QStringList bodyNames = m_loader->loadSolarSystem(contents, m_catalog);
        foreach (QString name, bodyNames)
        {
            Entity* e = m_catalog->find(name);
            if (e)
            {
                qDebug() << "Adding: " << name;
                m_view3d->replaceEntity(e, m_catalog->findInfo(name));
            }
        }
    }

    QSet<QString> resourceRequests = m_loader->resourceRequests();
    if (!resourceRequests.isEmpty())
    {
        qDebug() << "Resource requests:";
        foreach (QString resource, resourceRequests)
        {
            QNetworkRequest request(resource);
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);
            QNetworkReply* reply = m_networkManager->get(request);

            qDebug() << resource << " -> " << reply->url().toString();
        }
    }
}


void
Cosmographia::processReceivedResource(QNetworkReply* reply)
{
    qDebug() << "Resource received: " << reply->url().toString();

    QVariant fromCache = reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute);
    qDebug() << "Cached?" << fromCache.toBool();

    if (reply->open(QIODevice::ReadOnly))
    {
        QTextStream stream(reply);
        m_loader->processTleSet(reply->url().toString(), stream);
        m_loader->processUpdates();
    }
}
