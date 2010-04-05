#!/usr/bin/python
'''
  This code is distributed under the GPLv2 licence as a contribution to the Ufo Alien Invasion project.

  Coded by Dumper.
'''


import os
import sys
import xml.etree.ElementTree as ET
import ufo_parser

def get_names_dic(root,parent_dic):
  names_dic={}
  a=root.getiterator('item')
  for i in a:
    if parent_dic[i]==root:
      if len(i.getiterator('name'))==0:
        print ufo_parser.tostring(i)
      else:
        names_dic[i.text]=i.getiterator('name')[0].text.strip('_"')
  return names_dic

def get_firedefs(root,parent_dic):
  firedefs=[]
  #finding all firedefs:
  a=root.getiterator('firedef')
  for i in a:
    p=parent_dic[i]
    pp=parent_dic[p]
    if (p.tag=='weapon_mod'): #test to see if the parent is the expected type of node
      weapon=p.text
      item=pp.text
      firedefs.append([weapon,item,{}])
      for j in i:
        firedefs[-1][-1][j.tag]=j.text
  return firedefs

def get_protections_dic(root,parent_dic):
  #finding all armour:
  a=root.getiterator('protection')
  protections_dic={}
  for i in a:
    p=parent_dic[i]
    if p.tag=="item": #test to see if the parent is the expected type of node
      armour_data={}
      for j in i:
        armour_data[j.tag]=int(j.text)
      protections_dic[p.text]=armour_data
  return protections_dic


def read_double_field(s):
  r=s.strip('" ')
  r=(r+' ').split(' ')
  if(r[0]!=''):
    r[0]=int(r[0])
  else:
    r[0]=0
  if(r[1]!=''):
    r[1]=int(r[1])
  else:
    r[1]=0
  return [r[0],r[1]]

#Begin program:

#Reading files:
#Opens all ufo files in the folder "./../../base/ufos/" relative to the script path.
ufo_path=os.path.dirname(os.path.realpath(sys.argv[0]))+"/../../base/ufos/"
filenames=os.listdir(ufo_path)
s=''
for i in filenames:
  if (".ufo"==i[-4:].lower()):
    if os.path.isfile(ufo_path+i):
      f=open(ufo_path+i,'rb')
      s=s+'\n'+f.read()
      f.close()

#Parsing all:
root            = ufo_parser.parse_to_tree(s) #parses the ufos to a tree
parent_dic      = ufo_parser.generate_parent_dictionary(root) #
names_dic       = get_names_dic(root,parent_dic)
firedefs        = get_firedefs(root,parent_dic)
protections_dic = get_protections_dic(root,parent_dic)

#Adding 'None' armour:
protections_dic['none']={}
names_dic['none']='None'

#Printing column headers:
print 'weapon\tammo\tfiring mode\tskill\tspread\tcrouch\trange\tshots/use\tammo/use\tTUs/use\tdamage/shot\tsplash damage\tsplash radius\tdamage type\t',
for i in protections_dic.keys():
  #print names_dic[i]+' armour protection\t',
  #print names_dic[i]+' min damage/use\t',
  #print names_dic[i]+' max damage/use\t',
  print names_dic[i]+' average damage/use\t',
  print names_dic[i]+' average damage/TU\t',
print ''


for i in firedefs:
  #getting parameters:
  time=int(i[2]['time']) #time units to use
  shots=int(i[2]['shots'])  #shots in each use
  damage=read_double_field(i[2].setdefault('damage','0 0')) #damage by shot
  spldmg=read_double_field(i[2].setdefault('spldmg','0 0')) #splash damage (by shot?)
  dmgweight=i[2].setdefault('dmgweight','') #type of damage
  print names_dic[i[0]]+'\t',                          #weapon
  print names_dic[i[1]]+'\t',                          #ammo
  print i[2]['name'].strip('"_')+'\t',                 #firing mode
  print i[2].setdefault('skill','')+'\t',              #skill
  print i[2].setdefault('spread','').strip('"_')+'\t', #spread
  print i[2].setdefault('crouch','')+'\t',             #crouch
  print i[2].setdefault('range','')+'\t',              #range
  print i[2].setdefault('shots','')+'\t',              #shots/use
  print i[2].setdefault('ammo','')+'\t',               #ammo/use
  print i[2].setdefault('time','')+'\t',               #TUs/use
  print i[2].setdefault('damage','').strip('"_')+'\t', #damage/shot
  print i[2].setdefault('spldmg','').strip('"_')+'\t', #splash damage
  print i[2].setdefault('splrad','')+'\t',             #splash radius
  print i[2].setdefault('dmgweight','')+'\t',          #damage type
  for j in protections_dic.keys():
    protection=protections_dic[j].setdefault(dmgweight,'0') #absorption of this armor against that kind of damage
    #calculating damage in case no shots are missed:
    #splash damage and direct damage are added
    min_damage=int(shots) * (damage[0]-damage[1]+spldmg[0]-spldmg[1]-int(protection))
    max_damage=int(shots) * (damage[0]+damage[1]+spldmg[0]+spldmg[1]-int(protection))
    if(max_damage<0): # if max_damage is negative, we don't even bother
      avg_damage=0.
    elif (min_damage>=0): #if min_damage is positive, we just average min and max
      avg_damage=(max_damage+min_damage)/2.
    else: #we calculate the positive triangular area and divide by the full range.
      #since no shot can do negative damage, we need to be careful when calculating average damage in this case.
      #For example: the average damage of min_damage=-10 and max_damage=+10 is 2.62, not zero.
      avg_damage=( (max_damage*(max_damage+1))/2 )/(max_damage-min_damage+1.)
    #print str(protection)+'\t',                          #armour protection
    #print str(min_damage)+'\t',                          #min damage/use
    #print str(max_damage)+'\t',                          #max damage/use
    print str(round(avg_damage,1))+'\t',                 #average damage/use
    print str(round(avg_damage/time,1))+'\t',            #average damage/TU
  print ''
