/*

Copyright (C) 2012-2019 Michael Goffioul.
Copyright (C) 2012-2019 Jacob Dawid.

This file is part of QTerminal.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not,
see <https://www.gnu.org/licenses/>.

*/

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QKeySequence>
#include <QList>
#include <QMenu>
#include <QRegularExpression>
#include <QStringList>
#include <QWidget>

#include "gui-preferences-global.h"
#include "gui-preferences-cs.h"
#include "gui-preferences-sc.h"
#include "gui-settings.h"

#include "builtin-defun-decls.h"
#include "interpreter.h"

#include "QTerminal.h"
#if defined (Q_OS_WIN32)
# include "win32/QWinTerminalImpl.h"
#else
# include "unix/QUnixTerminalImpl.h"
#endif

QTerminal *
QTerminal::create (QWidget *p)
{
#if defined (Q_OS_WIN32)
  QTerminal *terminal = new QWinTerminalImpl (p);
#else
  QTerminal *terminal = new QUnixTerminalImpl (p);
#endif

  // FIXME: this function should probably be called from or part of the
  // QTerminal constructor?

  terminal->construct ();

  return terminal;
}

// slot for the terminal's context menu
void
QTerminal::handleCustomContextMenuRequested (const QPoint& at)
  {
    QClipboard * cb = QApplication::clipboard ();
    QString selected_text = selectedText();
    bool has_selected_text = ! selected_text.isEmpty ();

    _edit_action->setVisible (false);
    m_edit_selected_action->setVisible (false);
    m_help_selected_action->setVisible (false);
    m_doc_selected_action->setVisible (false);

#if defined (Q_OS_WIN32)
    // include this when in windows because there is no filter for
    // detecting links and error messages yet
    if (has_selected_text)
      {
        QRegularExpression file {"(?:[ \\t]+)(\\S+) at line (\\d+) column (?:\\d+)"};
        QRegularExpressionMatch match = file.match (selected_text);

        if (match.hasMatch ())
          {
            QString file_name = match.captured (1);
            QString line = match.captured (2);

            _edit_action->setVisible (true);
            _edit_action->setText (tr ("Edit %1 at line %2")
                                   .arg (file_name).arg (line));

            QStringList data;
            data << file_name << line;
            _edit_action->setData (data);
          }
      }
#endif

    if (has_selected_text)
      {
        // Find first word in selected text, trim everything else
        QRegularExpression expr {"(\\w+)"};
        QRegularExpressionMatch match = expr.match (selected_text);

        if (match.hasMatch ())
          {
            QString expr_found = match.captured (1);

            m_edit_selected_action->setVisible (true);
            m_edit_selected_action->setText (tr ("Edit \"%1\"").arg (expr_found));
            m_edit_selected_action->setData (expr_found);

            m_help_selected_action->setVisible (true);
            m_help_selected_action->setText (tr ("Help on \"%1\"").arg (expr_found));
            m_help_selected_action->setData (expr_found);
          }

        // Grab all of selected text, but trim leading non-word characters
        // and trailing whitespace
        expr.setPattern ("(\\w.*)\\s*$");
        match = expr.match (selected_text);
        if (match.hasMatch ())
          {
            QString expr_found = match.captured (1);

            m_doc_selected_action->setVisible (true);
            m_doc_selected_action->setText (tr ("Documentation on \"%1\"")
                                            .arg (expr_found));
            m_doc_selected_action->setData (expr_found);
          }
      }

    _paste_action->setEnabled (cb->text().length() > 0);
    _copy_action->setEnabled (has_selected_text);
    _run_selection_action->setVisible (has_selected_text);

    // Get the actions of any hotspots the filters may have found
    QList<QAction*> actions = get_hotspot_actions (at);
    if (actions.length ())
      _contextMenu->addSeparator ();
    for (int i = 0; i < actions.length (); i++)
      _contextMenu->addAction (actions.at(i));

    // Finally, show the context menu
    _contextMenu->exec (mapToGlobal (at));

    // Cleaning up, remove actions of the hotspot
    for (int i = 0; i < actions.length (); i++)
      _contextMenu->removeAction (actions.at(i));
  }

// slot for running the selected code
void
QTerminal::run_selection ()
{
  QStringList commands = selectedText ().split (QRegularExpression {"[\r\n]"},
#if defined (HAVE_QT_SPLITBEHAVIOR_ENUM)
                                                Qt::SkipEmptyParts);
#else
                                                QString::SkipEmptyParts);
#endif
  for (int i = 0; i < commands.size (); i++)
    emit execute_command_in_terminal_signal (commands.at (i));

}

// slot for edit files in error messages
void
QTerminal::edit_file ()
{
  QString file = _edit_action->data ().toStringList ().at (0);
  int line = _edit_action->data ().toStringList ().at (1).toInt ();

  emit edit_mfile_request (file,line);
}

// slot for edit selected function names
void QTerminal::edit_selected ()
{
  QString file = m_edit_selected_action->data ().toString ();

  emit edit_mfile_request (file,0);
}

// slot for showing help on selected epxression
void QTerminal::help_on_expression ()
{
  QString expr = m_help_selected_action->data ().toString ();

  emit execute_command_in_terminal_signal ("help " + expr);
}

// slot for showing documentation on selected epxression
void QTerminal::doc_on_expression ()
{
  std::string expr = m_doc_selected_action->data ().toString ().toStdString ();

  emit interpreter_event
    ([expr] (octave::interpreter& interp)
     {
       // INTERPRETER THREAD

       octave::F__event_manager_show_documentation__ (interp, ovl (expr));
     });
}

void
QTerminal::notice_settings ()
{
  octave::gui_settings settings;

  // Set terminal font:
  QFont term_font = QFont ();
  term_font.setStyleHint (QFont::TypeWriter);
  QString default_font = settings.string_value (global_mono_font);
  term_font.setFamily
    (settings.value (cs_font.settings_key (), default_font).toString ());
  term_font.setPointSize
    (settings.int_value (cs_font_size));
  setTerminalFont (term_font);

  QFontMetrics metrics (term_font);
  setMinimumSize (metrics.maxWidth ()*16, metrics.height ()*3);

  QString cursor_type
    = settings.string_value (cs_cursor);

  bool cursor_blinking;
  if (settings.contains (global_cursor_blinking.settings_key ()))
    cursor_blinking = settings.bool_value (global_cursor_blinking);
  else
    cursor_blinking = settings.bool_value (cs_cursor_blinking);

  for (int ct = IBeamCursor; ct <= UnderlineCursor; ct++)
    {
      if (cursor_type.toStdString () == cs_cursor_types[ct])
        {
          setCursorType ((CursorType) ct, cursor_blinking);
          break;
        }
    }

  bool cursorUseForegroundColor
    = settings.bool_value (cs_cursor_use_fgcol);

  int mode = settings.int_value (cs_color_mode);

  setForegroundColor (settings.color_value (cs_colors[0], mode));

  setBackgroundColor (settings.color_value (cs_colors[1], mode));

  setSelectionColor (settings.color_value (cs_colors[2], mode));

  setCursorColor (cursorUseForegroundColor,
                  settings.color_value (cs_colors[3], mode));

  setScrollBufferSize (settings.int_value (cs_hist_buffer));

  // If the Copy shortcut is Ctrl+C, then the Copy action also emits
  // a signal for interrupting the current code executed by the worker.
  // If the Copy shortcut is not Ctrl+C, an extra interrupt action is
  // set up for emitting the interrupt signal.

  QString sc = settings.sc_value (sc_main_edit_copy);

  //  Dis- or enable extra interrupt action: We need an extra option when
  //  copy shortcut is not Ctrl-C or when global shortcuts (like copy) are
  //  disabled.
  bool extra_ir_action
      = (sc != QKeySequence (Qt::ControlModifier | Qt::Key_C).toString ())
        || settings.bool_value (sc_prevent_rl_conflicts);

  _interrupt_action->setEnabled (extra_ir_action);
  has_extra_interrupt (extra_ir_action);

  // check whether shortcut Ctrl-D is in use by the main-window
  bool ctrld = settings.bool_value (sc_main_ctrld);
  _nop_action->setEnabled (! ctrld);
}

void
QTerminal::construct ()
{
  octave::gui_settings settings;

  // context menu
  setContextMenuPolicy (Qt::CustomContextMenu);

  _contextMenu = new QMenu (this);

  _copy_action
    = _contextMenu->addAction (settings.icon ("edit-copy"), tr ("Copy"), this,
                               SLOT (copyClipboard ()));

  _paste_action
    = _contextMenu->addAction (settings.icon ("edit-paste"), tr ("Paste"), this,
                               SLOT (pasteClipboard ()));

  _contextMenu->addSeparator ();

  _selectall_action
    = _contextMenu->addAction (tr ("Select All"), this, SLOT (selectAll ()));

  _run_selection_action
    = _contextMenu->addAction (tr ("Run Selection"), this,
                               SLOT (run_selection ()));

  m_edit_selected_action
    = _contextMenu->addAction (tr ("Edit selection"), this,
                               SLOT (edit_selected ()));
  m_help_selected_action
    = _contextMenu->addAction (tr ("Help on selection"), this,
                               SLOT (help_on_expression ()));
  m_doc_selected_action
    = _contextMenu->addAction (tr ("Documentation on selection"), this,
                               SLOT (doc_on_expression ()));

  _edit_action = _contextMenu->addAction (tr (""), this, SLOT (edit_file ()));

  _contextMenu->addSeparator ();

  _contextMenu->addAction (tr ("Clear Window"), this,
                           SIGNAL (clear_command_window_request ()));

  connect (this, SIGNAL (customContextMenuRequested (QPoint)),
           this, SLOT (handleCustomContextMenuRequested (QPoint)));

  // extra interrupt action
  _interrupt_action = new QAction (this);
  addAction (_interrupt_action);

  _interrupt_action->setShortcut
    (QKeySequence (Qt::ControlModifier | Qt::Key_C));
  _interrupt_action->setShortcutContext (Qt::WidgetWithChildrenShortcut);

  bool ok = connect (_interrupt_action, SIGNAL (triggered ()),
           this, SLOT (terminal_interrupt ()));

  // dummy (nop) action catching Ctrl-D in terminal, no connection
  _nop_action = new QAction (this);
  addAction (_nop_action);

  _nop_action->setShortcut (QKeySequence (Qt::ControlModifier | Qt::Key_D));
  _nop_action->setShortcutContext (Qt::WidgetWithChildrenShortcut);
}
