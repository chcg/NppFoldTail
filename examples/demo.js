// Select JavaScript language, then fold each block or use Fold All.
/* Short comment preview: slash-star, ellipsis, star-slash.
 * Hovering the ellipsis shows all these lines.
 */

/* Long comment preview.
 * line 2
 * line 3
 * line 4
 * line 5
 * line 6
 * line 7
 * line 8
 * line 9
 * line 10: last line shown in tooltip
 * line 11: omitted from tooltip
 */

function hello() {
    console.log("https://example.com");
} // Expected label: ... }

const settings = {
    enabled: true,
}; // Expected label: ... };

setTimeout(() => {
    console.log("callback");
}); // Expected label: ... });

setTimeout(()=>{ // opening comment: included in tooltip, hidden when folded
    let $totalContainer = $('#rightColumn_1');
},1000); // closing comment: included in tooltip, hidden when folded

function nested() {
    if (true) {
        console.log("nested");
    } // Inner label: ... }
} // Outer label: ... }
