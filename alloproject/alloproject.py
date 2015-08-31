#!/usr/bin/env python
# -*- coding: utf-8 -*-

# TODO decide how to bring in bugfixes from the alloproject scripts
# TODO figure out how to align the Dependency checkboxes!
# TODO Add xcode project generation for OS X
# TODO Add Windows support
# TODO Initialize GIT and make first commit of sources (optionally upload to github?)

import os
import platform
import stat
import urllib2
import pickle
from subprocess import Popen, PIPE

import Tkinter as tk
import tkFileDialog

class Project():
    def __init__(self, log_func):
        self.use_installed_libs = False
        self.dependencies = {}
        self.dependencies['GLV'] = True
        self.dependencies['Cuttlebone'] = True
        self.dependencies['Gamma'] = True
        self.dir = ''
        self.project_name = ''
        self.log_func = log_func
        
    def make_project_folder(self, folder, name):
        fullpath = folder + '/' + name
        if os.path.isdir(fullpath):
            return False
        if not os.path.exists(fullpath):
            os.makedirs(fullpath)
            self.log_func("Creating intermediate directories for project path.\n")
        else:
            os.mkdir(fullpath)
        if not os.path.isdir(fullpath):
            return False
        self.dir = folder
        self.project_name = name
        
        src_files = ['common.hpp','simpleApp.cpp', 'flags.cmake']
        curpath = os.getcwd()
        os.mkdir(fullpath + '/src')
        os.chdir(fullpath + '/src')
        for src_file in src_files:
            response = urllib2.urlopen('https://raw.githubusercontent.com/AlloSphere-Research-Group/AlloProject/master/src/%s'%src_file)
            f = open(fullpath + '/src/' + src_file, 'w')
            f.write(response.read())
        os.chdir(curpath)
        return True
        
    def make_init_script(self):
        script_text = '#!/bin/sh\n\n'
        files_to_get = ['CMakeLists.txt', 'run.sh', 'distclean']
        if platform.system() == "Darwin":
            files_to_get.append('makexcode.sh')
        for file_to_get in files_to_get:
            script_text += 'wget -N https://raw.githubusercontent.com/AlloSphere-Research-Group/AlloProject/master/%s\n'%file_to_get

        script_text += '\nchmod 750 run.sh\nchmod 750 distclean\n\n'
        cmake_modules = ['make_dep.cmake', 'CMakeRunTargets.cmake']
        script_text += 'mkdir -p cmake_modules\ncd cmake_modules\n'

        for module in cmake_modules:
            script_text += 'wget -N https://raw.githubusercontent.com/AlloSphere-Research-Group/AlloProject/master/cmake_modules/%s\n'%module
        
        script_text += '\ncd ..\n'
        if not self.use_installed_libs:
            script_text += 'git clone -b devel --depth 1 https://github.com/AlloSphere-Research-Group/AlloSystem.git AlloSystem\n'
            if self.dependencies['Cuttlebone']:
                script_text += 'git clone --depth 1 https://github.com/rbtsx/cuttlebone.git cuttlebone\n'
            if self.dependencies['Gamma']:
                script_text += 'git clone --depth 1 https://github.com/AlloSphere-Research-Group/Gamma.git Gamma\n'
            if self.dependencies['GLV']:
                script_text += 'git clone --depth 1 https://github.com/AlloSphere-Research-Group/GLV.git GLV\n'
        
        script_text += '''git fat init
git fat pull
'''
        
        curpath = os.getcwd()
        os.chdir(self.dir + '/' + self.project_name)
        f = open('initproject.sh', 'w')
        f.write(script_text)
        os.chmod('initproject.sh', stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR | stat.S_IRGRP | stat.S_IWGRP | stat.S_IXGRP)
        os.chdir(curpath)        
        
    def run_init_script(self):
        curpath = os.getcwd()
        os.chdir(self.dir + '/' + self.project_name)
        p = Popen(['./initproject.sh'], stdout=PIPE, stderr=PIPE, stdin=PIPE)
        stdout = []
        while True:
            line = p.stdout.readline()
            stdout.append(line)
            print line,
            if line == '' and p.poll() != None:
                break
        output = '\n'.join(stdout)
        os.chdir(curpath)
        
        return output
    

class Application(tk.Frame):
    def __init__(self, master=None):
        tk.Frame.__init__(self, master)
        self.grid(padx=10, pady=10, ipadx=5, ipady=10)
        self.project = Project(self.log)
        self.createWidgets()
        try:
            with open('.alloproject', 'rb') as config_file:
                settings = pickle.load(config_file)
                config_file.close()
                self.project_dir.delete(0, tk.END)
                self.project_dir.insert(0, settings['project_dir'])
        except:
            print("Not using config file")
        
    def createWidgets(self):
        self.name_label = tk.Label(self)
        self.name_label["text"] = "Project Name:"
        self.name_label.grid(row=0, column=0)
        
        self.project_name = tk.Entry(self, width = 40)
        self.project_name.grid(row=0, column=1)
        
        self.dir_label = tk.Label(self)
        self.dir_label["text"] = "Project Root Directory:"
        self.dir_label.grid(row=1, column=0)
        
        self.project_dir = tk.Entry(self, width = 40)
        self.project_dir.grid(row=1, column=1)
        self.project_dir.insert(0, os.getcwd())
        self.project_dir_select = tk.Button(self, text="...",
                                            command=self.select_directory)
        self.project_dir_select.grid(row=1, column=2)
        
        self.project_dir_help = tk.Button(self, text="?",
                                            command=self.project_dir_help_box)
        self.project_dir_help.grid(row=1, column=3)
        
        self.lib_frame = tk.Frame(self)
        self.lib_frame['borderwidth'] = 2
        self.lib_frame['relief'] = tk.RIDGE
        self.lib_frame.grid(row=5, column=0)

        self.dir_label = tk.Label(self.lib_frame)
        self.dir_label["text"] = "Base libraries:"
        self.dir_label.grid(row=4, column=0)

        self.libs_button_value = tk.IntVar()
        self.use_installed_libs = tk.Radiobutton(self.lib_frame,
                                                 text='Use installed libs',
                                                 variable=self.libs_button_value,
                                                 value=1,
                                                 command=self.set_libs_status)
        self.use_installed_libs.grid(row=5, column=0)
        self.get_libs = tk.Radiobutton(self.lib_frame,
                                       text='Download library sources',
                                       variable=self.libs_button_value,
                                       value = 0,
                                       command=self.set_libs_status)
        self.get_libs.grid(row=6, column=0)
        
        
        self.addlib_frame = tk.Frame(self)
        self.addlib_frame['borderwidth'] = 2
        self.addlib_frame['relief'] = tk.RIDGE
        self.addlib_frame.grid(row=5, column=1)
        
        self.lib_label = tk.Label(self.addlib_frame, text= "Additional libraries:")
        self.lib_label.grid(row=10, column=0)
        
        self.libs = []  
        self.glv = tk.Checkbutton(self.addlib_frame, text='GLV', anchor=tk.NW, justify=tk.LEFT)
        if self.project.dependencies['GLV']:       
            self.glv.select()
        else:
            self.glv.deselect()
        self.glv.grid(row=11, column=0)
        self.libs.append(self.glv)
        
        self.gamma = tk.Checkbutton(self.addlib_frame, text='Gamma', anchor=tk.NW, justify=tk.LEFT)
        if self.project.dependencies['Gamma']:       
            self.gamma.select()
        else:
            self.gamma.deselect()
        self.gamma.grid(row=12, column=0)
        self.libs.append(self.gamma)
        
        self.cuttlebone = tk.Checkbutton(self.addlib_frame, text='Cuttlebone', anchor=tk.NW, justify=tk.LEFT)
        if self.project.dependencies['Cuttlebone']:       
            self.cuttlebone.select()
        else:
            self.cuttlebone.deselect()    
        self.cuttlebone.grid(row=13, column=0)
        self.libs.append(self.cuttlebone)
        
        self.Accept = tk.Button(self, text="Make project", 
                                command=self.make_project,
                                width=60)
        self.Accept.grid(row=50, column=0, columnspan=3)
        
        self.console = tk.Text(self, height = 10)
        self.console.grid(row=60, column=0, columnspan=3, pady=10)
  
    def select_directory(self):
        project_dir = self.location_dialog = tkFileDialog.askdirectory(parent=root,
                                                                       initialdir=self.project_dir.get(),
                                                                       title='Please select a directory')
        #print "dir = ", project_dir
        if project_dir:
            self.project_dir.delete(0, tk.END)
            self.project_dir.insert(0, project_dir)
            
    def set_libs_status(self):
        if self.libs_button_value.get() == 0:
            self.project.use_installed_libs = False
            self.lib_label.grid()
            for lib in self.libs:
                lib.grid()
            
        elif self.libs_button_value.get() == 1:
            self.project.use_installed_libs = True
            self.lib_label.grid_remove()
            for lib in self.libs:
                lib.grid_remove()
        
    def make_project(self):
        name = self.project_name.get()
        folder = self.project_dir.get()
        if not name:
              wdw = tk.Toplevel()
              #wdw.geometry('+400+400')
              e = tk.Label(wdw, text='Please choose a project name')
              e.pack(ipadx=50, ipady=10, pady=10)
              b = tk.Button(wdw, text='OK', command=wdw.destroy)
              b.pack(ipadx=50, pady=10)
              wdw.transient(root)
              wdw.grab_set()
              root.wait_window(wdw)
              return
        self.console.insert(tk.END, 'Creating project -------\n')
        ok = self.project.make_project_folder(folder, name)
        if not ok:
            wdw = tk.Toplevel()
            #wdw.geometry('+400+400')
            e = tk.Label(wdw, text="Can't create project folder.")
            e.pack(ipadx=50, ipady=10, pady=10)
            b = tk.Button(wdw, text='OK', command=wdw.destroy)
            b.pack(ipadx=50, pady=10)
            wdw.transient(root)
            wdw.grab_set()
            root.wait_window(wdw)
            self.console.insert(tk.END, "ABORT: Couldn't create folder. Does it exist? Do you have permissions?\n")
            return
        self.project.make_init_script() 
        self.update_idletasks()
        self.console.insert(tk.END, 'Running init_project.sh (this might take a while) -------\n')
        self.update_idletasks()
        output = self.project.run_init_script()
        self.console.insert(tk.END, output + '\n')
        self.console.insert(tk.END, 'Project created succesfully.\n')
        settings = {}
        settings['project_dir'] = folder
        with open('.alloproject', 'wb') as config_file:
            pickle.dump(settings, config_file)
            config_file.close()
            
    def log(self, message):
        self.console.insert(tk.END, message)
        
    def project_dir_help_box(self):
        wdw = tk.Toplevel()
        #wdw.geometry('+400+400')
        e = tk.Label(wdw, text="A new folder will be created in this directory.")
        e.pack(ipadx=50, ipady=10, pady=10)
        b = tk.Button(wdw, text='OK', command=wdw.destroy)
        b.pack(ipadx=50, pady=10)
        wdw.transient(root)
        wdw.grab_set()
        root.wait_window(wdw)
        return

root = tk.Tk()
root.title('AlloProject')
app = Application(master=root)
app.mainloop()




