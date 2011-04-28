#-*- encoding: UTF-8 -*-

import sys
import os
import argparse
from lxml import html
from lxml import etree
from lxml.etree import Comment
import subprocess
import cStringIO
import re
from shutil import copy, copytree, ignore_patterns, rmtree

def nt2posix(relative_path):
    return relative_path.replace('\\', '/') # posixpath.normpath do nothing

class CssResMapper(object):
    """Rebind resources in minimized CSS"""
    def __init__(self, css_full_name, css_output_dir, html_input_dir):
        self.css_input_dir = os.path.split(css_full_name)[0]
        self.html_input_dir = html_input_dir
        self.css_output_dir = css_output_dir
        
    def __call__(self, match):
        original_res_path_relative_to_css = os.path.normpath( match.group(1).strip('"\'') )
        full_res_path = os.path.join(self.css_input_dir, original_res_path_relative_to_css)

        if os.path.exists(full_res_path):
            # if resource is available, copy it to res output dir and change path to it(relative to html_output_dir).
            relative_res_dir = os.path.relpath(self.css_input_dir, self.html_input_dir)            
            new_res_filename = os.path.abspath( os.path.join(self.css_output_dir, relative_res_dir, original_res_path_relative_to_css) )
            new_res_dir = os.path.split(new_res_filename)[0]
            if not os.path.exists(new_res_filename):                
                if not os.path.exists(new_res_dir):
                    os.makedirs(new_res_dir)
                copy(full_res_path, new_res_filename)

            new_css_url = nt2posix( os.path.relpath(new_res_filename, self.css_output_dir) )
        else:
            # if resource is not available, print warnings and not.
            new_css_url = match.group(1)
            print self.css_input_dir
            print 'full_res_path %s does not exists' % full_res_path
            print 'original_res_path %s does not exists' % original_res_path

        return 'url(%s)' % new_css_url

def main():
    ERROR_CODE = 1
    SUCCESS_CODE = 0
    
    # create parser of program arguments.
    parser = argparse.ArgumentParser(
        description = 'Join all javascripts and css files used in HTML file in one file which contains their minified versions.',
        epilog = ''
    )

    parser.add_argument('input_html', nargs=1,      action='store',                help='HTML file name to optimize.')
    parser.add_argument('-output',                  action='store', required=True, help='optimized HTML folder.')
    parser.add_argument('-google_closure_compiler', action='store', required=True, help='Path to google closure compiler applet.')
    parser.add_argument('-yuicompressor',           action='store', required=True, help='Path to yuicompressor applet.')
    args = parser.parse_args()

    html_filename = args.input_html[0]
    input_html_dir = os.path.abspath( os.path.split(html_filename)[0] )
    output_html_dir = os.path.abspath( args.output.strip('"') )
    # prevent overwriting of source html if output dir is the same as input dir.
    if input_html_dir == output_html_dir:
        output_html_dir = os.path.join(output_html_dir, 'release')
    # create output dir if needed
    if not os.path.exists(output_html_dir):
        os.makedirs(output_html_dir)
    #print output_html_dir
    doc = html.parse(html_filename)
    head_element = doc.getroot().find('head')

    script_elements_to_replace = []
    scripts_to_merge_paths_list = [ [] ]
    # fill list of lists of scripts for merge.
    # Example:
    # <script type="text/javascript" src="script1.js"/>
    # <script type="text/javascript" src="script2.js"/>
    # <script language="javascript">foo();</script>
    # <script type="text/javascript" src="script3.js"/>
    # will be merged following:
    # <script type="text/javascript" src="js_script0.js"/>
    # <script language="javascript">foo();</script>
    # <script type="text/javascript" src="js_script1.js"/>
    # where js_script0.js is concatination of script1.js and script2.js, js_script1.js is script3.js
    for script in list( head_element.iter('script') ):
        scripts_to_merge_paths = scripts_to_merge_paths_list[-1]
        if script.get('type') == 'text/javascript' or script.get('language') == 'javascript':
            script_src = script.get('src')
            if script_src != None:
                full_script_path = os.path.join(input_html_dir, script_src)
                #print full_script_path
                if os.path.exists(full_script_path):                    
                    if len(scripts_to_merge_paths) == 0:
                        script_elements_to_replace.append(script)
                    else:
                        head_element.remove(script)
                    scripts_to_merge_paths.append(full_script_path)
                else:
                    scripts_to_merge_paths_list.append([])
            else:
                scripts_to_merge_paths_list.append([])        

    # merge javascripts.
    merged_files_data = {} # maps script filepath to content.
    merged_script_name_template = os.path.join(output_html_dir, 'js_script')
    merged_script_index = 0
    for scripts_to_merge_paths in scripts_to_merge_paths_list:        
        if len(scripts_to_merge_paths) > 0:
            merged_script_name = '%s%d.js' % (merged_script_name_template, merged_script_index)
            
            # include merged script in HTML file.
            script_element = script_elements_to_replace[merged_script_index]
            script_element.clear()
            script_element.set('type', 'text/javascript')            
            script_element.set( 'src', nt2posix( os.path.relpath(merged_script_name, output_html_dir) ) )
            # write all scripts of group in one script.
            output_file = cStringIO.StringIO()            
            for script_to_merge in scripts_to_merge_paths:
                #print script_to_merge
                output_file.write( open(script_to_merge, 'r').read() )
                print >> output_file, ''
            # store merged script content in dictionary.
            merged_files_data[merged_script_name] = output_file.getvalue()
            output_file.close()
            merged_script_index = merged_script_index + 1

    def getHandledDataFromProcess(cmd, input_data):
        #return input_data ###!!!
        """send input data to stdin of process pointed by cmd then read and return it's stdout."""
        PIPE = subprocess.PIPE       
        p = subprocess.Popen(cmd, shell=True, stdin=PIPE, stdout=PIPE, stderr=PIPE) # assign None to stderr to view errors on console.
        output_data = p.communicate(input_data)[0] # return stdout of launched process.
        return output_data
    
    # minimize javascripts.
    
    js_compressor_cmd = 'java -jar %s --compilation_level SIMPLE_OPTIMIZATIONS' % args.google_closure_compiler # js_script0.js 270kb # ADVANCED_OPTIMIZATIONS breaks code since it removes unused code. Try it with one big file.
    # js_compressor_cmd = 'java -jar yuicompressor-2.4.2.jar --type js --charset utf-8' # js_script0.js 304kb
    for filename in merged_files_data:        
        merged_file = open(filename, 'w')                
        merged_file.write( getHandledDataFromProcess(js_compressor_cmd, merged_files_data[filename]) ) # output stdout of google closure compiler.
        merged_file.close()

    # merge css.
    merged_css_name = os.path.join(output_html_dir, 'css0.css')
    merged_css_data = cStringIO.StringIO()
    for css in list( head_element.iter('link') ):
        if css.get('type') == 'text/css':
            css_src = css.get('href')
            if css_src != None:
                full_css_path = os.path.join(input_html_dir, css_src)
                #print full_css_path
                if os.path.exists(full_css_path):                    
                    if merged_css_data.tell() == 0:
                        css.clear()
                        css.set('type', 'text/css')
                        css.set('rel', 'stylesheet')
                        css.set( 'href', nt2posix( os.path.relpath(merged_css_name, output_html_dir) ) ) 
                    else:
                        head_element.remove(css)

                    css_content = open(full_css_path, 'r').read()
                    merged_css_data.write( re.sub('url\((.*?)\)', CssResMapper(full_css_path, output_html_dir, input_html_dir), css_content) )
                    print >> merged_css_data, ''

    # minimize css.
    css_compressor_cmd = 'java -jar %s --type css --charset utf-8' % args.yuicompressor
    merged_css_file = open(merged_css_name, 'w')        
    merged_css_file.write( getHandledDataFromProcess(css_compressor_cmd, merged_css_data.getvalue() ) ) # output stdout of yui_compressor.
    merged_css_file.close()    
    merged_css_data.close()

    # make actions independent from HTML content but required for correct work of scripts.
    makeCustomActions(input_html_dir, output_html_dir)

    # remove comments from html.
    for element in list( doc.getroot().iter(Comment) ): # iterate through copy of list since we need to remove elements from original list
        #print html.tostring(element)
        element.getparent().remove(element)
    
    # create new html file
    new_html_path = os.path.join( output_html_dir, os.path.split(html_filename)[1] )
    html_file = open(new_html_path, 'w')
    print >> html_file, doc.docinfo.doctype
    print >> html_file, html.tostring(doc, pretty_print=True, include_meta_content_type=True, encoding='utf-8')
    html_file.close()
    
    return SUCCESS_CODE

def makeCustomActions(input_html_dir, output_html_dir):
    """make actions independent from HTML content but required for correct work of scripts."""
    
    # copy favicon to output dir
    copy(os.path.join(input_html_dir, 'favicon.ico'), output_html_dir)
    # copy i18n folder to output dir
    dirname = 'i18n'
    output_i18n = os.path.join(output_html_dir, dirname)
    if os.path.exists(output_i18n):
        rmtree(output_i18n)
        
    copytree(os.path.join(input_html_dir, dirname), output_i18n, ignore=ignore_patterns('.svn', 'CVS'))

if __name__ == '__main__':
    error_code = main()
    #sys.exit(error_code); # comment when debug script in IDLE.
