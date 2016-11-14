/*

Copyright (C) 2016 Andrew Thornton

This file is part of Octave.

Octave is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Octave is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Octave; see the file COPYING.  If not, see
<http://www.gnu.org/licenses/>.

*/

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <QAbstractButton>
#include <QButtonGroup>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QRadioButton>
#include <QTimer>

#include "Canvas.h"
#include "Container.h"
#include "ContextMenu.h"
#include "ButtonGroup.h"
#include "ToggleButtonControl.h"
#include "RadioButtonControl.h"
#include "Backend.h"
#include "QtHandlesUtils.h"

#include "ov-struct.h"

namespace QtHandles
{

  static int
  frameStyleFromProperties (const uibuttongroup::properties& pp)
  {
    if (pp.bordertype_is ("none"))
      return QFrame::NoFrame;
    else if (pp.bordertype_is ("etchedin"))
      return (QFrame::Box | QFrame::Sunken);
    else if (pp.bordertype_is ("etchedout"))
      return (QFrame::Box | QFrame::Raised);
    else if (pp.bordertype_is ("beveledin"))
      return (QFrame::Panel | QFrame::Sunken);
    else if (pp.bordertype_is ("beveledout"))
      return (QFrame::Panel | QFrame::Raised);
    else
      return (QFrame::Panel | QFrame::Plain);
  }

  static void
  setupPalette (const uibuttongroup::properties& pp, QPalette &p)
  {
    p.setColor (QPalette::Window,
                Utils::fromRgb (pp.get_backgroundcolor_rgb ()));
    p.setColor (QPalette::WindowText,
                Utils::fromRgb (pp.get_foregroundcolor_rgb ()));
    p.setColor (QPalette::Light,
                Utils::fromRgb (pp.get_highlightcolor_rgb ()));
    p.setColor (QPalette::Dark,
                Utils::fromRgb (pp.get_shadowcolor_rgb ()));
  }

  static int
  borderWidthFromProperties (const uibuttongroup::properties& pp)
  {
    int bw = 0;

    if (! pp.bordertype_is ("none"))
      {
        bw = octave::math::round (pp.get_borderwidth ());
        if (pp.bordertype_is ("etchedin") || pp.bordertype_is ("etchedout"))
          bw *= 2;
      }

    return bw;
  }

  ButtonGroup*
  ButtonGroup::create (const graphics_object& go)
  {
    Object* parent = Object::parentObject (go);

    if (parent)
      {
        Container* container = parent->innerContainer ();

        if (container)
          {
            QFrame* frame = new QFrame(container);
            return new ButtonGroup (go, new QButtonGroup (frame), frame);
          }
      }

    return 0;
  }

  ButtonGroup::ButtonGroup (const graphics_object& go, QButtonGroup* buttongroup,
                            QFrame* frame)
    : Object (go, frame), m_hiddenbutton(0), m_container (0), m_title (0),
      m_blockUpdates (false)
  {
    uibuttongroup::properties& pp = properties<uibuttongroup> ();

    frame->setObjectName ("UIButtonGroup");
    frame->setAutoFillBackground (true);
    Matrix bb = pp.get_boundingbox (false);
    frame->setGeometry (octave::math::round (bb(0)), octave::math::round (bb(1)),
                        octave::math::round (bb(2)), octave::math::round (bb(3)));
    frame->setFrameStyle (frameStyleFromProperties (pp));
    frame->setLineWidth (octave::math::round (pp.get_borderwidth ()));
    QPalette pal = frame->palette ();
    setupPalette (pp, pal);
    frame->setPalette (pal);
    m_buttongroup = buttongroup;
    m_hiddenbutton = new QRadioButton (frame);
    m_hiddenbutton->hide ();
    m_buttongroup->addButton (m_hiddenbutton);

    m_container = new Container (frame);
    m_container->canvas (m_handle);

    if (frame->hasMouseTracking ())
      {
        foreach (QWidget* w, frame->findChildren<QWidget*> ())
        { w->setMouseTracking (true); }
        foreach (QWidget* w, buttongroup->findChildren<QWidget*> ())
        { w->setMouseTracking (true); }
      }

    QString title = Utils::fromStdString (pp.get_title ());
    if (! title.isEmpty ())
      {
        m_title = new QLabel (title, frame);
        m_title->setAutoFillBackground (true);
        m_title->setContentsMargins (4, 0, 4, 0);
        m_title->setPalette (pal);
        m_title->setFont (Utils::computeFont<uibuttongroup> (pp, bb(3)));
      }

    frame->installEventFilter (this);
    m_container->installEventFilter (this);

    if (pp.is_visible ())
      {
        QTimer::singleShot (0, frame, SLOT (show (void)));
        QTimer::singleShot (0, buttongroup, SLOT (show (void)));
      }
    else
      frame->hide ();

    connect (m_buttongroup, SIGNAL (buttonClicked (QAbstractButton*)),
             SLOT (buttonClicked (QAbstractButton*)));
  }

  ButtonGroup::~ButtonGroup (void)
  { }

  bool
  ButtonGroup::eventFilter (QObject* watched, QEvent* xevent)
  {
    if (! m_blockUpdates)
      {
        if (watched == qObject ())
          {
            switch (xevent->type ())
              {
              case QEvent::Resize:
                {
                  gh_manager::auto_lock lock;
                  graphics_object go = object ();

                  if (go.valid_object ())
                    {
                      if (m_title)
                        {
                          const uibuttongroup::properties& pp =
                            Utils::properties<uibuttongroup> (go);

                          if (pp.fontunits_is ("normalized"))
                            {
                              QFrame* frame = qWidget<QFrame> ();

                              m_title->setFont (Utils::computeFont<uibuttongroup>
                                                (pp, frame->height ()));
                              m_title->resize (m_title->sizeHint ());
                            }
                        }
                      updateLayout ();
                    }
                }
                break;

              case QEvent::MouseButtonPress:
                {
                  QMouseEvent* m = dynamic_cast<QMouseEvent*> (xevent);

                  if (m->button () == Qt::RightButton)
                    {
                      gh_manager::auto_lock lock;

                      ContextMenu::executeAt (properties (), m->globalPos ());
                    }
                }
                break;

              default:
                break;
              }
          }
        else if (watched == m_container)
          {
            switch (xevent->type ())
              {
              case QEvent::Resize:
                if (qWidget<QWidget> ()->isVisible ())
                  {
                    gh_manager::auto_lock lock;

                    properties ().update_boundingbox ();
                  }
                break;

              default:
                break;
              }
          }
      }

    return false;
  }

  void
  ButtonGroup::update (int pId)
  {
    uibuttongroup::properties& pp = properties<uibuttongroup> ();
    QFrame* frame = qWidget<QFrame> ();

    m_blockUpdates = true;

    switch (pId)
      {
      case uibuttongroup::properties::ID_POSITION:
        {
          Matrix bb = pp.get_boundingbox (false);

          frame->setGeometry (octave::math::round (bb(0)), octave::math::round (bb(1)),
                              octave::math::round (bb(2)), octave::math::round (bb(3)));
          updateLayout ();
        }
        break;

      case uibuttongroup::properties::ID_BORDERWIDTH:
        frame->setLineWidth (octave::math::round (pp.get_borderwidth ()));
        updateLayout ();
        break;

      case uibuttongroup::properties::ID_BACKGROUNDCOLOR:
      case uibuttongroup::properties::ID_FOREGROUNDCOLOR:
      case uibuttongroup::properties::ID_HIGHLIGHTCOLOR:
      case uibuttongroup::properties::ID_SHADOWCOLOR:
        {
          QPalette pal = frame->palette ();

          setupPalette (pp, pal);
          frame->setPalette (pal);
          if (m_title)
            m_title->setPalette (pal);
        }
        break;

      case uibuttongroup::properties::ID_TITLE:
        {
          QString title = Utils::fromStdString (pp.get_title ());

          if (title.isEmpty ())
            {
              if (m_title)
                delete m_title;
              m_title = 0;
            }
          else
            {
              if (! m_title)
                {
                  QPalette pal = frame->palette ();

                  m_title = new QLabel (title, frame);
                  m_title->setAutoFillBackground (true);
                  m_title->setContentsMargins (4, 0, 4, 0);
                  m_title->setPalette (pal);
                  m_title->setFont (Utils::computeFont<uibuttongroup> (pp));
                  m_title->show ();
                }
              else
                {
                  m_title->setText (title);
                  m_title->resize (m_title->sizeHint ());
                }
            }
          updateLayout ();
        }
        break;

      case uibuttongroup::properties::ID_TITLEPOSITION:
        updateLayout ();
        break;

      case uibuttongroup::properties::ID_BORDERTYPE:
        frame->setFrameStyle (frameStyleFromProperties (pp));
        updateLayout ();
        break;

      case uibuttongroup::properties::ID_FONTNAME:
      case uibuttongroup::properties::ID_FONTSIZE:
      case uibuttongroup::properties::ID_FONTWEIGHT:
      case uibuttongroup::properties::ID_FONTANGLE:
        if (m_title)
          {
            m_title->setFont (Utils::computeFont<uibuttongroup> (pp));
            m_title->resize (m_title->sizeHint ());
            updateLayout ();
          }
        break;

      case uibuttongroup::properties::ID_VISIBLE:
        frame->setVisible (pp.is_visible ());
        updateLayout ();
        break;

      case uibuttongroup::properties::ID_SELECTEDOBJECT:
        {
          graphics_handle h = pp.get_selectedobject ();
          gh_manager::auto_lock lock;
          graphics_object go = gh_manager::get_object (h);
          Object* selectedObject = Backend::toolkitObject (go);
          ToggleButtonControl* toggle = static_cast<ToggleButtonControl*>
                                        (selectedObject);
          RadioButtonControl* radio = static_cast<RadioButtonControl*>(selectedObject);
          if (toggle)
            {
              go.get_properties ().set ("value", 1);
            }
          else if (radio)
            {
              go.get_properties ().set ("value", 1);
            }
          else
            {
              m_hiddenbutton->setChecked (true);
            }
        }
        break;

      default:
        break;
      }

    m_blockUpdates = false;
  }

  void
  ButtonGroup::redraw (void)
  {
    Canvas* canvas = m_container->canvas (m_handle);

    if (canvas)
      canvas->redraw ();
  }

  void
  ButtonGroup::updateLayout (void)
  {
    uibuttongroup::properties& pp = properties<uibuttongroup> ();
    QFrame* frame = qWidget<QFrame> ();

    Matrix bb = pp.get_boundingbox (true);
    int bw = borderWidthFromProperties (pp);

    frame->setFrameRect (QRect (octave::math::round (bb(0)) - bw,
                                octave::math::round (bb(1)) - bw,
                                octave::math::round (bb(2)) + 2*bw, octave::math::round (bb(3)) + 2*bw));
    m_container->setGeometry (octave::math::round (bb(0)),
                              octave::math::round (bb(1)),
                              octave::math::round (bb(2)), octave::math::round (bb(3)));

    if (m_blockUpdates)
      pp.update_boundingbox ();

    if (m_title)
      {
        QSize sz = m_title->sizeHint ();
        int offset = 5;

        if (pp.titleposition_is ("lefttop"))
          m_title->move (bw+offset, 0);
        else if (pp.titleposition_is ("righttop"))
          m_title->move (frame->width () - bw - offset - sz.width (), 0);
        else if (pp.titleposition_is ("leftbottom"))
          m_title->move (bw+offset, frame->height () - sz.height ());
        else if (pp.titleposition_is ("rightbottom"))
          m_title->move (frame->width () - bw - offset - sz.width (),
                         frame->height () - sz.height ());
        else if (pp.titleposition_is ("centertop"))
          m_title->move (frame->width () / 2 - sz.width () / 2, 0);
        else if (pp.titleposition_is ("centerbottom"))
          m_title->move (frame->width () / 2 - sz.width () / 2,
                         frame->height () - sz.height ());
      }
  }

  void
  ButtonGroup::selectNothing (void)
  {
    m_hiddenbutton->setChecked (true);
  }


  void
  ButtonGroup::addButton (QAbstractButton* btn)
  {
    m_buttongroup->addButton(btn);
    connect (btn, SIGNAL (toggled (bool)), SLOT (buttonToggled (bool)));
  }

  void
  ButtonGroup::buttonToggled (bool toggled)
  {
    Q_UNUSED (toggled);
    if (! m_blockUpdates)
      {
        gh_manager::auto_lock lock;
        uibuttongroup::properties& bp = properties<uibuttongroup> ();

        graphics_handle oldValue = bp.get_selectedobject();

        QAbstractButton* checkedBtn = m_buttongroup->checkedButton();

        graphics_handle newValue = graphics_handle ();
        if (checkedBtn != m_hiddenbutton)
          {
            Object* checkedObj = Object::fromQObject(checkedBtn);
            newValue = checkedObj->properties ().get___myhandle__ ();
          }

        if (oldValue != newValue)
          gh_manager::post_set (m_handle, "selectedobject", newValue.as_octave_value (),
                                false);
      }
  }

  void
  ButtonGroup::buttonClicked (QAbstractButton* btn)
  {
    Q_UNUSED(btn);

    gh_manager::auto_lock lock;
    uibuttongroup::properties& bp = properties<uibuttongroup> ();

    graphics_handle oldValue = bp.get_selectedobject();

    QAbstractButton* checkedBtn = m_buttongroup->checkedButton();
    Object* checkedObj = Object::fromQObject(checkedBtn);
    graphics_handle newValue = checkedObj->properties ().get___myhandle__ ();

    if (oldValue != newValue)
      {
        octave_scalar_map eventData;
        eventData.setfield ("OldValue", oldValue.as_octave_value ());
        eventData.setfield ("NewValue", newValue.as_octave_value ());
        eventData.setfield ("Source", bp.get___myhandle__ ().as_octave_value ());
        eventData.setfield ("EventName", "SelectionChanged");
        octave_value selectionChangedEventObject = octave_value (new octave_struct (
              eventData));
        gh_manager::post_callback(m_handle, "selectionchangedfcn",
                                  selectionChangedEventObject);
      }
  }

};

