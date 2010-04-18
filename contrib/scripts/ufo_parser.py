'''
  This code is distributed under the GPLv2 licence as a contribution to the Ufo Alien Invasion project.

  Coded by Dumper.
'''

import xml.etree.ElementTree as ET

def __clean_comments(s):
  '''
    Internal function. Strips comments from a string in the .ufo format.
    Used in function __split_into_tokens
  '''
  #cleaning // comments:
  a=s.split('\n')
  b=[]
  for i in a:
    c1=i.find('//')
    c2=i.find('"')
    while(c2>=0 and c2<c1):
      c2=i.find('"',c2+1)
      if(c2<0): c2=len(i)-1
      c1=i.find('//',c2)
      c2=i.find('"',c2+1)
    if c1>=0:
      t=i[:c1].rstrip('\r\n\t ')
    else:
      t=i.rstrip('\r\n\t ')
    if(t!=''):
      b.append(t)
  a='\n'.join(b)
  b=''
  #cleaning /* */ comments:
  #WARNING: this implementation does not allow for /* or */ appearing between quotes.
  t0=a.find('/*')
  while t0>=0:
    t1=t0
    t2=a.find('*/',t0+1)
    if(t2<0): t2=len(a)+1
    while t1 < t2:
     t0=t1
     t1=a.find('/*',t1+1)
     if(t1<0): t1=len(a)+1
    #print 'eliminated:'
    #print a[t2+2:t2+10]
    a=a[:t0]+a[t2+2:]
    t0=a.find('/*')
  return a

def __split_into_tokens(s):
  '''
    Internal function. Splits string s into a list of tokens.
    Used in function parse_to_tree
  '''
  s1=__clean_comments(s)
  a=s1.split('"')
  tokens=[]
  for i in range(len(a)):
    if i%2==0: #even
      temp=a[i].replace('\n',' \n ').replace('\t',' ').replace('{',' { ').replace('}',' } ')
      tokens=tokens+temp.split(' ')
    else: #odd. append text between quotes without splitting or replacing.
      tokens.append('"'+a[i]+'"')
      #tokens.append(a[i])
  b=[]
  for i in tokens:
    if i!='':
      b.append(i)
  return b

def parse_to_tree(s):
  '''
    Parses the contents of a .ufo file in string s to a tree using xml.etree.ElementTree.
  '''
  t=__split_into_tokens(s)
  root=ET.Element("ufo")
  cursor=root
  next=True
  lastnode=cursor
  node_track=[]
  for i in t:
    if i=='\n':
      next=True
    elif i=='{':
      node_track.append(cursor)
      cursor=lastnode
      next=True
    elif i=='}':
      cursor=node_track.pop()
      lastnode=cursor
      next=True
    else:
      if next==True:
        lastnode = ET.SubElement(cursor, i)
        next=False
      else:
	if lastnode.text:
          lastnode.text=lastnode.text+' '+i
        else:
          lastnode.text=i
  return root

def generate_parent_dictionary(root):
  '''
    Generates parent dictionary.
    This is necessary because elementtree nodes do not have parent pointers.
  '''
  parent_dic={}
  for i in root.getiterator():
    for j in i.getchildren():
      parent_dic[j]=i
  return parent_dic


def tostring(node):
  '''
    Translates a node into a more readable string. Used for debugging.
  '''
  def node_level(n,root,parent_dic):
    l=0
    while(n!=root and l<100):
      n=parent_dic[n]
      l=l+1
    return l
  def readtext(n):
    if n.text:
      return n.text
    else:
      return ''
  s=''
  parent_dic={}
  for i in node.getiterator():
    for j in i.getchildren():
      parent_dic[j]=i
  for i in node.getiterator():
    s=s+node_level(i,node,parent_dic)*'  '+str(i.tag)+' '+readtext(i)+'\n'
  return s
