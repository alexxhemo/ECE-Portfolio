// We want to use PUT operation type and a response type of success

import './LogActivity.css'
import { useState } from 'react';
import ReactDatePicker from '../components/TimePicker';

export default function LogActivity() {
  const [selectedOption, setSelectedOption] = useState('');
  const [hours, setHours] = useState('');
  const needsHoursInput = ['sleep_wokeup', 'sleep_bed', 'nap', 'exercise'].includes(selectedOption);
  const [selectedDate, setSelectedDate] = useState(null);

  const handleChange = (event) => {
    setSelectedOption(event.target.value);
  };

  const handleSubmit = () => {
    console.log("Submit button clicked!");
    console.log("Selected Option:", selectedOption);
    console.log("Selected Date:", selectedDate);
    alert("Activity has been logged");
  };

  return (
  <div className="log-activity">
      <div className="background-layer">
          <div className="rectangle purple"></div>
          <div className="rectangle magenta"></div>
          <div className="rectangle blue"></div>
      </div>

      <h1>Log Activity</h1>
      <label htmlFor="options">Choose an option:</label>
      <select id="options" value={selectedOption} onChange={handleChange}>
        <option value="">--Please choose an option--</option>
        <option value="sleep_wokeup">Woke up</option>
        <option value="sleep_bed">Went to bed</option>
        <option value="nap">Took a nap</option>
        <option value="exercise">Exercised</option>
        <option value="consume_food">Ate food</option>
        <option value="consume_caffeine">Consumed caffeine</option>
        <option value="consume_alcohol">Consumed alcohol</option>
      </select>

      <p>Selected: {selectedOption}</p>

      {needsHoursInput && (
        <div>
          <label htmlFor="hours">Enter hours:</label>
          <input
            type="number"
            id="hours"
            min="0"
            step="0.1"
            value={hours}
            onChange={(e) => setHours(e.target.value)}
            placeholder="e.g. 1.5"
          />
        </div>
      )}

      <ReactDatePicker 
        selectedDate={selectedDate}
        setSelectedDate={setSelectedDate}
      />
      <div className="button-layout">
        <button
            className="button-style"
            onClick={handleSubmit}
            disabled={!selectedOption || !selectedDate || (needsHoursInput && !hours)}
        >
            Submit
        </button>
      </div>
  </div>
  
  );
}