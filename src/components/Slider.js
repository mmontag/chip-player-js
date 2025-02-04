import React, {PureComponent} from 'react';
import autoBindReact from 'auto-bind/react';

export default class Slider extends PureComponent {
  constructor(props) {
    super(props);
    autoBindReact(this);

    this.node = React.createRef();
    this.knob = React.createRef();
    this.state = {
      dragging: false,
      draggedPos: null,
    };
  }

  onMouseMove(event) {
    if (this.state.dragging) {
      const node = this.node.current;
      const knob = this.knob.current;
      const frac = (event.clientX - node.offsetLeft - knob.offsetWidth / 2) / node.offsetWidth;
      const pos = Math.max(Math.min(frac, 1), 0);
      this.setState({
        draggedPos: pos,
      });
      this.props.onDrag(pos);
    }
  }

  onMouseDown(event) {
    event.preventDefault();
    event.persist();
    document.addEventListener('mousemove', this.onMouseMove);
    document.addEventListener('mouseup', this.onMouseUp);
    this.setState({dragging: true}, () => this.onMouseMove(event));
  }

  onMouseUp(event) {
    document.removeEventListener('mouseup', this.onMouseUp);
    document.removeEventListener('mousemove', this.onMouseMove);
    // Wait a moment to prevent 'snapback' (pos momentarily won't match draggedPos)
    setTimeout(() => {
      this.setState({dragging: false});
    }, 150);
    this.props.onChange(this.state.draggedPos);
  }

  render() {
    const pos = Math.max(Math.min((this.state.dragging ? this.state.draggedPos : this.props.pos), 1), 0) * 100 + '%';
    return (
      <div ref={this.node}
           className="Slider"
           onMouseDown={this.onMouseDown}>
        <div className="Slider-rail"/>
        <div className="Slider-knob"
             ref={this.knob}
             style={{left: pos}}/>
      </div>
    );
  }
}
