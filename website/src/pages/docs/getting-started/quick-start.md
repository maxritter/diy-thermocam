---
title: Quick Start
weight: 2
seo:
  type: stackbit_page_meta
  template: stackbit_page_meta
  title: Quick Start
  description: This is the quick start page
  extra:
    - name: 'og:type'
      value: website
      keyName: property
    - name: 'og:title'
      value: Quick Start
      keyName: property
    - name: 'og:description'
      value: This is the quick start page
      keyName: property
    - name: 'twitter:card'
      value: summary
    - name: 'twitter:title'
      value: Quick Start
    - name: 'twitter:description'
      value: This is the quick start page
template: docs
---

<div class="note">
  <strong>Note:</strong> 
  This is the demo content for demonstration purpose only. The primary function of this content is to show you what this theme can do. There is a more detailed explanation in the <strong>Getting Started</strong> section.
</div>

## Callouts

<hr>

There are two types of callouts available in this theme, **note** and **important**. To add a callout to your documentation simply add the following `html` code with class `important` or `note`. Like in the example bellow. 

### HTML example

Copy the code and modify these blocks according to your needs.

```html
<div class="important">
  <strong>Important:</strong> 
  This is an "Important" callout block of text.
  This block indicates a warning or caution. 
  Use it for an important message. 
</div>
```

```html
<div class="note">
  <strong>Note:</strong> 
  This is a "Note" callout block of text. 
  This block signifies a general note.
</div>
```
### Live example

<div class="important">
  <strong>Important:</strong> 
  This is an "Important" callout block of text. 
  This block indicates a warning or caution.
  Use it for an important message. 
</div>

<div class="note">
  <strong>Note:</strong> 
  This is an "Note" callout block of text. 
  This block signifies a general note.
</div>

## Code blocks

<hr>

<div class="note">
  <strong>Note:</strong>
  Code blocks have copy functionality. To copy the code press the grey button located in the right side top corner.
</div>

You can create simple code blocks by placing triple backticks <code>```</code> before and after the code block. To render a code block more readable, we recommend placing a blank line before and after code blocks.

<pre>```
if (condition) {
  code to run if condition is true
} else {
  run some other code instead
}
```
</pre>

```
if (condition) {
  code to run if condition is true
} else {
  run some other code instead
}
```

### Syntax highlighting

You can add a language identifier to enable syntax highlighting in your code block. For example, to syntax highlight **JavaScript** code, specify `javascript` next to the tick marks before the fenced code block:

<pre>
```javascript
if (condition) {
  code to run if condition is true
} else {
  run some other code instead
}
```
</pre>

The rendered output looks like this:

```javascript
if (condition) {
  code to run if condition is true
} else {
  run some other code instead
}
```

## Tables

<hr>

You can build tables with markdown to help you organize information. To add a table, use three or more hyphens (---) to create each column’s header, and use pipes (|) to separate each column like in the example below.

<pre>
| Title | Title |
| ------| ----- |
| Text  | Text  |
| Text  | Text  |
</pre>

| Title | Title |
| ------| ----- |
| Text  | Text  |
| Text  | Text  |

<div class="note">
  <strong>Note:</strong> 
  Creating tables with hyphens and pipes can be time-consuming. To speed up the process, try using the <a href="http://www.tablesgenerator.com/markdown_tables" >Markdown Tables Generator</a>.
</div>

## Start using Libris theme

<hr>

We’ve packed this theme with powerful features to help you have awesome documentation for your current or next project.

<br>

**Why not start using this theme today?**

<a href="https://www.stackbit.com/" class="button">Join Stackbit</a>
