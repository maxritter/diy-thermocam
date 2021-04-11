---
title: Home
sections:
  - type: section_hero
    template: section_hero
    title: Hero Section
    section_id: hero
    image: images/5.jpg
    content: >-
      This section can contain a subtitle or tagline. The recommended length is
      one to three sentences, but can be changed as you prefer.
    actions:
      - type: action
        template: action
        label: Get Started
        url: /docs
        style: primary
  - type: section_grid
    template: section_grid
    section_id: features
    col_number: three
    grid_items:
      - type: grid_item
        template: grid_item
        title: Documentation
        content: >-
          Donec lobortis velit sed suscipit lobortis. Ut non quam metus. Nullam
          a maximus mi. Quisque justo nunc, sollicitudin euismod euismod at,
          tincidunt ut tellus. Vivamus rhoncus mattis varius.
        actions:
          - type: action
            template: action
            label: Get Started
            url: /docs
            style: link
      - type: grid_item
        template: grid_item
        title: Blog
        content: >-
          Vestibulum a nunc ut eros condimentum posuere. Nullam dapibus quis
          nunc non interdum. Pellentesque tortor ligula, gravida ac commodo eu.
        actions:
          - type: action
            template: action
            label: View Posts
            url: /blog
            style: link
      - type: grid_item
        template: grid_item
        title: Style Guide
        content: >-
          Donec lobortis velit sed suscipit lobortis. Ut non quam metus. Nullam
          a maximus mi. Quisque justo nunc, sollicitudin euismod euismod at,
          tincidunt ut tellus. Vivamus rhoncus mattis varius.
        actions:
          - type: action
            template: action
            label: Learn More
            url: /style-guide
            style: link
  - type: section_content
    template: section_content
    title: A Section With An Image
    section_id: text-img
    image: images/jamstack.svg
    image_position: left
    content: >-
      Nam pulvinar ante eu ultricies volutpat. Sed nulla nibh, dapibus sit amet
      cursus quis, fringilla nec sapien. Vestibulum imperdiet nunc bibendum
      consectetur lobortis.
    actions:
      - type: action
        template: action
        label: View Demo
        url: /docs/getting-started
        style: primary
      - type: action
        template: action
        label: Get Started
        url: /docs/getting-started
        style: secondary
  - type: section_content
    template: section_content
    title: A Section Without Image
    section_id: text-no-img
    content: >-
      Nam pulvinar ante eu ultricies volutpat. Sed nulla nibh, dapibus sit amet
      cursus quis, fringilla nec sapien. Vestibulum imperdiet nunc bibendum
      consectetur lobortis.
    actions:
      - type: action
        template: action
        label: Get Started
        url: /docs/getting-started/installation
        style: primary
  - type: section_grid
    template: section_grid
    title: Sample Layouts
    section_id: features-two-col
    subtitle: An optional subtitle of the section
    col_number: two
    grid_items:
      - type: grid_item
        template: grid_item
        title: Overview
        content: >-
          Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec nisl
          ligula, cursus id molestie vel, maximus aliquet risus. Vivamus in nibh
          fringilla, fringilla tortor at, pulvinar orci.
        actions:
          - type: action
            template: action
            label: Learn More
            url: /overview
            style: link
      - type: grid_item
        template: grid_item
        title: Showcase
        content: >-
          Donec lobortis velit sed suscipit lobortis. Ut non quam metus. Nullam
          a maximus mi. Quisque justo nunc, sollicitudin euismod euismod at,
          tincidunt ut tellus. Vivamus rhoncus mattis varius.
        actions:
          - type: action
            template: action
            label: Learn More
            url: /showcase
            style: link
  - type: section_cta
    template: section_cta
    title: The Title of The Call to Action Block
    section_id: cta
    subtitle: This is an optional description for the call to action block.
    actions:
      - type: action
        template: action
        label: Get Started
        url: /docs/getting-started/installation
        style: primary
seo:
  type: stackbit_page_meta
  template: stackbit_page_meta
  title: Stackbit Libris Theme
  description: The preview of the Libris theme
  extra:
    - name: 'og:type'
      value: website
      keyName: property
    - name: 'og:title'
      value: Stackbit Libris Theme
      keyName: property
    - name: 'og:description'
      value: The preview of the Libris theme
      keyName: property
    - name: 'og:image'
      value: images/4.jpg
      keyName: property
      relativeUrl: true
    - name: 'twitter:card'
      value: summary_large_image
    - name: 'twitter:title'
      value: Stackbit Libris Theme
    - name: 'twitter:description'
      value: The preview of the Libris theme
    - name: 'twitter:image'
      value: images/4.jpg
      relativeUrl: true
template: advanced
---
