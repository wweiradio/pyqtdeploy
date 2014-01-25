# Copyright (c) 2014 Riverbank Computing Limited.
#
# This file is part of pyqtdeploy.
#
# This file may be used under the terms of the GNU General Public License
# v2 or v3 as published by the Free Software Foundation which can be found in
# the files LICENSE-GPL2.txt and LICENSE-GPL3.txt included in this package.
# In addition, as a special exception, Riverbank gives you certain additional
# rights.  These rights are described in the Riverbank GPL Exception, which
# can be found in the file GPL-Exception.txt in this package.
#
# This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
# WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.


from PyQt5.QtCore import QPoint, QSettings, QSize
from PyQt5.QtGui import QKeySequence
from PyQt5.QtWidgets import QFileDialog, QMainWindow, QMessageBox

from ..project import Project, ProjectException


class ProjectGUI(QMainWindow):
    """ The GUI for a project. """

    # The filter string to use with file dialogs.
    file_dialog_filter = "Projects (*.pdy)"

    def __init__(self):
        """ Initialise the GUI for an empty project. """

        super().__init__()

        self._set_project(Project())

        self._create_menus()
        self._load_settings()

    def load(self, filename):
        """ Load a project from the given file. """

        try:
            self._set_project(Project.load(filename))
        except ProjectException as e:
            self._handle_exception(e, "Open")

    def closeEvent(self, event):
        """ Handle a close event. """

        if self._current_project_done():
            self._save_settings()
            event.accept()
        else:
            event.ignore()

    def _set_project(self, project):
        """ Set the GUI's project. """

        self._project = project

        self._project.modified_changed.connect(self.setWindowModified)
        self._project.name_changed.connect(self._name_changed)

        self._set_window_title(self._project.name)

    def _set_window_title(self, name):
        """ Set the window title. """

        if name == "":
            name = "Unnamed"

        self.setWindowTitle(name + '[*]')

    def _name_changed(self, name):
        """ Invoked when the project's name changes. """

        self._set_window_title(name)
        self._update_actions()

    def _create_menus(self):
        """ Create the menus. """

        file_menu = self.menuBar().addMenu("&File")
        file_menu.addAction("&New", self._new_project, QKeySequence.New)
        file_menu.addAction("&Open...", self._open_project, QKeySequence.Open)
        self._save_action = file_menu.addAction("&Save", self._save_project,
                QKeySequence.Save)
        file_menu.addAction("Save &As...", self._save_as_project,
                QKeySequence.SaveAs)
        file_menu.addSeparator()
        file_menu.addAction("E&xit", self.close, QKeySequence.Quit)

        self._update_actions()

    def _update_actions(self):
        """ Update the state of the actions. """

        self._save_action.setEnabled(self._project.name != "")

    def _new_project(self):
        """ Create a new, unnamed project. """

        if self._current_project_done():
            self._set_project(Project())

    def _open_project(self):
        """ Open an existing project. """

        if self._current_project_done():
            filename, _ = QFileDialog.getOpenFileName(self, "Open",
                    filter=self.file_dialog_filter)

            if filename != '':
                self.load(filename)

    def _save_project(self):
        """ Save the project and return True if it was saved. """

        try:
            self._project.save()
        except ProjectException as e:
            self._handle_exception(e, "Save")
            return False

        return True

    def _save_as_project(self):
        """ Save the project under a new name and return True if it was saved.
        """

        filename, _ = QFileDialog.getSaveFileName(self, "Save As",
                    filter=self.file_dialog_filter)

        if filename == '':
            return False

        try:
            self._project.save_as(filename)
        except ProjectException as e:
            self._handle_exception(e, "Save")
            return False

        return True

    def _handle_exception(self, e, title):
        """ Handle a ProjectException. """

        msg_box = QMessageBox(QMessageBox.Warning, title, e.text, parent=self)

        if e.detail != '':
            msg_box.setDetailedText(e.detail)

        msg_box.exec()

    def _current_project_done(self):
        """ Return True if the user has finished with any current project. """

        if self._project.modified:
            msg_box = QMessageBox(QMessageBox.Question, "Save",
                    "The project has been modified.",
                    QMessageBox.Save|QMessageBox.Discard|QMessageBox.Cancel,
                    parent=self)

            msg_box.setDefaultButton(QMessageBox.Save)
            msg_box.setInformativeText("Do you want to save your changes?")

            ans = msg_box.exec()

            if ans == QMessageBox.Cancel:
                return False

            if ans == QMessageBox.Save:
                return self._save_project() if self._project.name != "" else self._save_as_project()

        return True

    def _load_settings(self):
        """ Load the user specific settings. """

        settings = QSettings()

        self.resize(settings.value('size', QSize(400, 400)))
        self.move(settings.value('pos', QPoint(200, 200)))

    def _save_settings(self):
        """ Save the user specific settings. """

        settings = QSettings()

        settings.setValue('size', self.size())
        settings.setValue('pos', self.pos())
