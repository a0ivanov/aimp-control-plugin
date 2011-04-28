/*
 * jScroller2 1.61 - Scroller Script
 *
 * Copyright (c) 2008 Markus Bordihn (markusbordihn.de)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * $Date: 2009-07-16 18:00:00 +0100 (Thu, 16 July 2009) $
 * $Rev: 1.61 $
 */
 
/*global window*/
/*jslint bitwise: true, browser: true, eqeqeq: true, immed: true, newcap: true, regexp: true, undef: true */

var ByRei_jScroller2 = {
 info: {
  Name: "ByRei jScroller2",
  Version: 1.61,
  Author: "Markus Bordihn (http://markusbordihn.de)",
  Description: "Next Generation Autoscroller"
 },

 config: {
  refreshtime: 150,
  regExp: {
   px: /([0-9,.\-]+)px/
  }
 },

 obj: [],
 /* 
   Object List Struction 
   ===============================================================================
   [0 / parent].height:[Number]        - Height of the Parent Container DIV
   [0 / parent].width:[Number]         - Width of the Parent Container DIV
   [1 / child].obj:[Object]            - Content DIV / Child DIV
   [1 / child].height:[Number]         - Height of Content DIV
   [1 / child].width:[Number]          - Width of Content DIV
   [2 / endless].obj:[Object]          - Second Content DIV for endless function
   [2 / endless].height:[Number]       - Height of second Content DIV
   [2 / endless].width:[Number]        - Width of second Content DIV
   [3 / option].direction:[Text]       - set the dirction (up,down,left,right)
   [3 / option].speed:[Number]         - set the speed of the scrolling
   [3 / option].pause:[Boolean]        - pause the scroller so it will not start
   [3 / option].delay:[Number]         - delay the startet of the scroller in sec.
   [3 / option].alternate:[Boolean]    - alternate scrolling
   [3 / option].dynamic:[Boolean]      - dynamic checking on every request
   ===============================================================================
  */

 cache: {
  active: false,
  prefix: 'jscroller2' + '_',
  delayer: 0,
  ileave: 0,
  ie: window.detachEvent ? true : false,
  last : {
   element : false
  }
 },

 get: {
  /* Check an Array for certain values and return the result */
  value: function (obj,value,ncs) {
   var
    i,
    result='',
    il = obj.length;
   if (obj && value) {
       for (i=0;i<il;i++) {
            if (ncs && obj[i].indexOf(value) >= 0) {
                result=obj[i].split(value)[1];
                break;
            } else if (obj[i] === value) {
                result=obj[i];
                break;
            }
       }
   }
   return result;
  },
  /* Get px value */
  px: function(obj) {
   var result = "";
   if (obj) {
       if (obj.match(ByRei_jScroller2.config.regExp.px)) {
           if (typeof obj.match(ByRei_jScroller2.config.regExp.px)[1] !== 'undefined') {
               result = obj.match(ByRei_jScroller2.config.regExp.px)[1];
           }
       }
   }
   return parseFloat(result);
  },
  /* Complexe Ckecking for Endless DIV return and set position */
  endless: function(mode, direction, value, obj, child, parent, endless, speed, alternate) {
   var result;

   switch (mode) {
    case 'down': case 'right':
     result = ByRei_jScroller2.get.px(obj.style[direction]) + speed;
     if (value > 0 && value <= parent) {
         ByRei_jScroller2.set[direction](obj, value - endless);
     }
     if (result + endless >= parent && result <= parent + speed) {
         ByRei_jScroller2.set[direction](obj, result);
         value = result + child * -1;
     }
    return value;
    case 'up': case 'left':
     result = ByRei_jScroller2.get.px(obj.style[direction]) - speed;
     if (value + child <= parent) {
         ByRei_jScroller2.set[direction](obj, value + child);
     }
     if (result + endless <= parent && result + endless + speed >= 0) {
         ByRei_jScroller2.set[direction](obj, result);
         value = result + endless;
     }
    return value;
   }
  }
 },

 /* 
  This include all on(action) Methodes 
 */
 on: {
  /* Set Event Handler when no action is required */
  blur: function() {
   if (ByRei_jScroller2.cache.last.element && ByRei_jScroller2.cache.last.element !== document.activeElement) {
       ByRei_jScroller2.cache.last.element = document.activeElement;
   } else {
       ByRei_jScroller2.stop();
   }
  },
  /* Set Event Handler when action is required */
  focus: function() {
   ByRei_jScroller2.start();
  },
  /* Set Event Handler for delayed starts */
  delay: function (delay) {
   if (delay > 0) {
       for (var i=0;i<ByRei_jScroller2.obj.length;i++) {
            if (delay === ByRei_jScroller2.obj[i][3].delay) {
                ByRei_jScroller2.obj[i][3].pause = ByRei_jScroller2.obj[i][3].delay = 0;
            }
       }
   }
  },
  /* Set Event Handler for Object Mouse Over */
  over: function(evt) {
   if (evt) {
       ByRei_jScroller2.start_stop(evt,1);
   }
  },
  /* Set Event Handler for Object Mouse Out */
  out: function(evt) {
   if (evt) {
       ByRei_jScroller2.start_stop(evt,0);
   }
  }  
 },

  /* 
  All set Functions
 */
 set: {
  /* Simple Warper to avoid to include "px" on every value */
  left: function(obj,value) {
   ByRei_jScroller2._style(obj,'left',value + "px");
  },
  top: function(obj,value) {
   ByRei_jScroller2._style(obj,'top',value + "px");
  },
  width: function(obj,value) {
   ByRei_jScroller2._style(obj,'width',value + "px");
  },
  height: function(obj,value) {
   ByRei_jScroller2._style(obj,'height',value + "px");
  }
 },

  /*
  This include all init functions 
 */
 init: {
  /* Init Events for all div with dynDiv_ as className */
  main: function() {
   var 
    div_list = document.getElementsByTagName('div'),
    il = div_list.length,
    i;

   /* Check if some Object have a direction */
   for (i=0;i<il;i++) {
    var 
     classNames = div_list[i].className.split(' '),
     direction = null;

    if (ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'down')) {direction = 'down';}
    else if (ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'up')) {direction = 'up';}
    else if (ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'left')) {direction = 'left';}
    else if (ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'right')) {direction = 'right';}

    if (direction) {
        ByRei_jScroller2.add(div_list[i],direction);
    }
   }
   if (!ByRei_jScroller2.active) {
       if (ByRei_jScroller2.obj.length > 0) {
           ByRei_jScroller2.start();
           if (ByRei_jScroller2.cache.delayer) {
               for (i=0;i<ByRei_jScroller2.obj.length;i++) {
                 if (ByRei_jScroller2.obj[i][3].delay > 0) {
                     window.setTimeout("ByRei_jScroller2.on.delay(" + ByRei_jScroller2.obj[i][3].delay + ",0)", ByRei_jScroller2.obj[i][3].delay);
                 }
               } 
           }
           if (ByRei_jScroller2.cache.ileave === 0) {
               if (ByRei_jScroller2.cache.ie) {
                   ByRei_jScroller2.cache.last.element = document.activeElement;
                   ByRei_jScroller2.set_eventListener(document, 'focusout',  ByRei_jScroller2.on.blur);
               } else {
                   ByRei_jScroller2.set_eventListener(window, 'blur',  ByRei_jScroller2.on.blur);
               }
               ByRei_jScroller2.set_eventListener(window, 'focus', ByRei_jScroller2.on.focus);
               ByRei_jScroller2.set_eventListener(window, 'resize', ByRei_jScroller2.on.focus);
               ByRei_jScroller2.set_eventListener(window, 'scroll', ByRei_jScroller2.on.focus);
           }
       }
   }
  }
 },

 /* Add needed DIVs to the obj List after some checks */
 add: function(obj, direction) {
  var
   i,
   il = ByRei_jScroller2.obj.length,
   error = false;

  /* Check for duplication, when Object is existing the direction will be replace nothing more to do... */
  if (obj && direction) {  
      if (il > 0) {
          for (i=0;i<il;i++) {
           if (ByRei_jScroller2.obj[i][1].obj === obj) {
               ByRei_jScroller2.obj[i][3].direction = direction;
               error=true;
           }
         }
      }
  } else {
    error=true;
  }
  if (!error) {
     var
      delay = 0, 
      speed = 1,
      pause = 0,
      alternate,
      dynamic,
      classNames = obj.className.split(' '),
      parent = obj.parentNode,
      endless = {
       obj : null,
       width : null,
       height : null
      };

     if (parent.className.indexOf('jscroller2') >= 0) {
         parent = parent.parentNode;
     }

     if (parent) {
         ByRei_jScroller2._style(parent,'position','relative');
         ByRei_jScroller2._style(parent,'overflow','hidden');

         var childs = parent.getElementsByTagName('div');
         for (i=0;i<childs.length;i++) {
              if (ByRei_jScroller2.get.value(childs[i].className.split(' '), ByRei_jScroller2.cache.prefix + direction +'_endless')) {
                  endless.obj = childs[i];
              }
         }

        if(obj) { 
           ByRei_jScroller2._style(obj,'position','absolute');
           ByRei_jScroller2.set.top(obj,0);
           ByRei_jScroller2.set.left(obj,0);

           switch(direction) {
            case "down":  
             ByRei_jScroller2.set.top(obj,(obj.clientHeight * -1) +  parent.clientHeight);
            break;  
            case "right":
             ByRei_jScroller2.set.left(obj,(obj.clientWidth * -1) +  parent.clientWidth);
            break;
           }

           switch(direction) {
            case "down":  case "up": 
             ByRei_jScroller2.set.width(obj, parent.clientWidth);
            break;
            case "right": case "left":
             ByRei_jScroller2.set.height(obj, parent.clientHeight);
            break;
           }

           if(endless.obj) {
              ByRei_jScroller2._style(endless.obj,'position','absolute');
              endless.width = endless.obj.clientWidth;
              endless.height = endless.obj.clientHeight;

              switch(direction) {
               case "down":
                ByRei_jScroller2.set.top(endless.obj, endless.height * -1);
               break;
               case "up": 
                ByRei_jScroller2.set.top(endless.obj, obj.clientHeight);
               break;
               case "left": 
                ByRei_jScroller2.set.left(endless.obj, obj.clientWidth);
               break;
               case "right":
                ByRei_jScroller2.set.left(endless.obj, obj.clientWidth* -1);
               break;
              }

              switch(direction) {
               case "down": case "up":
                ByRei_jScroller2.set.left(endless.obj, 0);
                ByRei_jScroller2.set.width(endless.obj, parent.clientWidth);
               break;
               case "left": case "right":
                ByRei_jScroller2.set.top(endless.obj, 0);
                ByRei_jScroller2.set.height(endless.obj, parent.clientHeight);
               break;
              }
           }
        }

        /* Speed Parameter */
        if (ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'speed-',1)) {
            speed = parseFloat(ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'speed-',1)||10)/10;
            if (ByRei_jScroller2.cache.ie && speed < 1) {speed = 1;}
        }

        /* Ignore Leave */
        ByRei_jScroller2.cache.ileave = (ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'ignoreleave') || ByRei_jScroller2.cache.ileave === 1) ? 1 : 0;

        /* Alternate */
        alternate = ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'alternate') ? 1 : 0;

        /* Dynamic */
        dynamic = ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'dynamic') ? 1 : 0;

        /* Scroller Delay Start */
        if (ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'delay-',1)) {
            ByRei_jScroller2.cache.delayer = pause = 1;
            delay = ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'delay-',1) * 1000;
        }

        /* Stop & Start on MouseOver */
        if (ByRei_jScroller2.get.value(classNames, ByRei_jScroller2.cache.prefix + 'mousemove')) {
            ByRei_jScroller2.set_eventListener(obj, 'mouseover', ByRei_jScroller2.on.over);
            ByRei_jScroller2.set_eventListener(obj, 'mouseout', ByRei_jScroller2.on.out);
            if (endless.obj) {
                ByRei_jScroller2.set_eventListener(endless.obj, 'mouseover', ByRei_jScroller2.on.over);
                ByRei_jScroller2.set_eventListener(endless.obj, 'mouseout', ByRei_jScroller2.on.out);
            }
        }
        ByRei_jScroller2.obj.push([
         {height: parent.clientHeight, width: parent.clientWidth},                                                // Parent  [0]
         {obj: obj, height: obj.clientHeight, width: obj.clientWidth},                                            // Child   [1]
         {obj: endless.obj, height: endless.height, width: endless.width},                                        // Endless [2]
         {direction: direction, speed: speed, pause: pause, delay: delay, alternate: alternate, dynamic: dynamic} // Options [3]
        ]);
     }
   }
 },

 /* Remove jScroller2 Object from the List complett */
 remove: function(obj) {
  if (obj) {
     for (var i=0;i<ByRei_jScroller2.obj.length;i++) {
         if (ByRei_jScroller2.obj[i][1].obj === obj) {
             ByRei_jScroller2.obj.splice(i, 1);
         }
     }
     if (ByRei_jScroller2.obj.length <= 0) {
         ByRei_jScroller2.stop();
     }
  }
 },

 /* Main Part - Scroller Events which will execute for a special Internval */
 scroller: function() {
  var
   i,
   il = ByRei_jScroller2.obj.length;

  for (i=0;i<il;i++) {
    var
     parent  = ByRei_jScroller2.obj[i][0],
     child   = ByRei_jScroller2.obj[i][1],
     endless = ByRei_jScroller2.obj[i][2],
     option  = ByRei_jScroller2.obj[i][3];

    if (!option.pause && !option.delay) {

      // check size and change the size, important for option.dynamic Objects...
       if (option.dynamic) {
           child.height = ByRei_jScroller2.obj[i][1].height = child.obj.clientHeight;
           child.width = ByRei_jScroller2.obj[i][1].width = child.obj.clientWidth;

           if (endless.obj) {
               endless.height = ByRei_jScroller2.obj[i][2].height = endless.obj.clientHeight;
               endless.width = ByRei_jScroller2.obj[i][2].width = endless.obj.clientWidth;
           }
       }

       switch(option.direction) {
             /* Calculations for option.direction "down" and "up" */
             case 'down': case 'up':
                var new_top = ByRei_jScroller2.get.px(child.obj.style.top);
                    new_top = (option.alternate === 2) ? ((option.direction === 'down') ? new_top - option.speed : new_top + option.speed) : ((option.direction === 'down') ? new_top + option.speed : new_top - option.speed);

                if(endless.obj && !option.alternate) {
                   new_top = ByRei_jScroller2.get.endless(option.direction, 'top', new_top, endless.obj, child.height, parent.height, endless.height, option.speed, option.alternate);
                }
                else {
                   if (option.alternate) {
                        if (option.alternate === ((option.direction === 'down') ? 1 : 2) && ((child.height > parent.height && new_top + option.speed > 0) || (child.height < parent.height && new_top + child.height + option.speed > parent.height))) {ByRei_jScroller2.obj[i][3].alternate = ((option.direction === 'down') ? 2 : 1);}
                        if (option.alternate === ((option.direction === 'down') ? 2 : 1) && ((child.height > parent.height && new_top + child.height < parent.height + option.speed) || (child.height < parent.height && new_top < 0))) {ByRei_jScroller2.obj[i][3].alternate = ((option.direction === 'down') ? 1 : 2);}
                   } else {
                       if (option.direction === 'down') {
                           if (new_top > parent.width) {new_top = (child.height) * -1;}
                       } else {
                           if (new_top < child.height * -1) {new_top = parent.height;}
                       }
                   }
                }
                ByRei_jScroller2.set.top(child.obj, new_top);
             break;
             /* Calculations for option.direction "left" and "right" */
             case 'left': case 'right':
              var new_left = ByRei_jScroller2.get.px(child.obj.style.left);
                  new_left = (option.alternate === 2) ? ((option.direction === 'left') ? new_left + option.speed : new_left - option.speed) : (option.direction === 'left') ? new_left - option.speed : new_left + option.speed;

              if(endless.obj && !option.alternate) {
                 new_left = ByRei_jScroller2.get.endless(option.direction, 'left', new_left, endless.obj, child.width, parent.width, endless.width, option.speed, option.alternate);
              } else {
                 if (option.alternate) {
                    if (option.alternate === ((option.direction === 'left') ? 2 : 1) && ((child.width > parent.width && new_left + option.speed > 0) || (child.width < parent.width && new_left + child.width + option.speed > parent.width))) {ByRei_jScroller2.obj[i][3].alternate = ((option.direction === 'left') ? 1 : 2);}
                    if (option.alternate === ((option.direction === 'left') ? 1 : 2) && ((child.width > parent.width && new_left + child.width < parent.width + option.speed) || (child.width < parent.width && new_left - option.speed < 0))) {ByRei_jScroller2.obj[i][3].alternate = ((option.direction === 'left') ? 2 : 1);}
                 } else {
                    if (option.direction === 'left') {
                        if (new_left < child.width * -1) {new_left = parent.width;}
                    } else {
                        if (new_left > parent.width) {new_left = (child.width) * -1;}
                    }
                 }
              }
              ByRei_jScroller2.set.left(child.obj, new_left);
             break;
       }
   }
  }
 },


 /* Start or Stop a specific Scroller */
 start_stop: function (evt,mode){
  if (evt.target || evt.srcElement) {
   var evt_src = evt.target ? evt.target : evt.srcElement;
   for (var i=0;i<5;i++) {
    if (evt_src.className.indexOf( ByRei_jScroller2.cache.prefix + 'mousemove') < 0 && evt_src.className.indexOf('_endless') < 0) {
        evt_src = evt_src.parentNode;
       } else {
        break;
       }
   }
   ByRei_jScroller2.pause(evt_src,mode);
  }
 },

 /* Start all Scrollers */
 start: function() {
  if (!ByRei_jScroller2.timer) {
      ByRei_jScroller2.active = ByRei_jScroller2.timer = window.setInterval(ByRei_jScroller2.scroller, ByRei_jScroller2.config.refreshtime);
  }
 },

 /* Stop all Scrollers */
 stop: function() {
  if (ByRei_jScroller2.timer) {
      window.clearInterval(ByRei_jScroller2.timer);
      ByRei_jScroller2.active = ByRei_jScroller2.timer = false;
  }
 },
 
 /* Pause a specific Scroller */
 pause: function (obj, value) {
  if (obj && value >= 0) {
      for (var i=0;i<ByRei_jScroller2.obj.length;i++) {
           if (obj === ByRei_jScroller2.obj[i][1].obj || obj === ByRei_jScroller2.obj[i][2].obj) {
               ByRei_jScroller2.obj[i][3].pause = value;
           }
      }
  }
 },

 /* Small alternative Style Selector to avoid JS Errors and make things easier */
 _style: function(obj,o_style,value) {
  if (obj && o_style) {
     if (obj.style) {
         if (typeof obj.style[o_style] !== 'undefined') {
            if (value) {
                try {return (obj.style[o_style] = value);} catch(e) {return false;}
            } else {
                return (obj.style[o_style] === '') ? ((obj.currentStyle) ? obj.currentStyle[o_style] : ((window.getComputedStyle) ? window.getComputedStyle(obj,'').getPropertyValue(o_style) : false)) : obj.style[o_style];
            }
         }
     }
  }
 },

 /* Set Standart Event Listener */
 set_eventListener: function(obj,evt,func) {
  if (obj && evt && func) {
   if (ByRei_jScroller2.cache.ie) {obj.attachEvent("on"+evt, func);} else {obj.addEventListener(evt, func, false);}
  }
 }

};

/* Init the hole jScroller2 after the Document was loaded complet */
ByRei_jScroller2.set_eventListener(window, 'load', ByRei_jScroller2.init.main);