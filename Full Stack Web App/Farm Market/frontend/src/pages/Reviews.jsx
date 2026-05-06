import React, { useState, useEffect } from 'react';
import { reviewsApi } from '../api';
import { AppShell, StarRating } from '../components';
import { useAuth } from '../context/AuthContext';

function ReviewCard({ review, isSeller }) {
  return (
    <div className="card" style={{ padding: 20, marginBottom: 12 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: 12 }}>
        <div>
          <div style={{ fontWeight: 600, fontSize: 15, marginBottom: 4 }}>
            {isSeller
              ? (review.buyer_name || 'Anonymous Buyer')
              : (review.farm_name || 'Unknown Farm')}
          </div>
          <div style={{ fontSize: 12, color: 'var(--gray-600)' }}>
            Order #{review.order_id} · {review.created_at
              ? new Date(review.created_at).toLocaleDateString('en-US', {
              month: 'short',
              day: 'numeric',
              year: 'numeric',
            })
          : 'Unknown date'}
          </div>
        </div>
        <StarRating value={review.rating} readOnly />
      </div>
      {review.comment && (
        <p style={{ fontSize: 14, color: 'var(--gray-800)', lineHeight: 1.6, margin: 0 }}>
          "{review.comment}"
        </p>
      )}
    </div>
  );
}

export default function Reviews() {
  const { user } = useAuth();
  const [reviews, setReviews] = useState([]);
  const [avgRating, setAvgRating] = useState(null);
  const [reviewCount, setReviewCount] = useState(0);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  useEffect(() => {
    const fetchReviews = async () => {
      setLoading(true);
      try {
        setError('');
        setReviews([]);
        setAvgRating(null);
        setReviewCount(0);

        if (!user) return;

        if (user.role === 'seller') {
          const data = await reviewsApi.bySeller(user.id);
          setReviews(Array.isArray(data?.reviews) ? data.reviews : []);
          setAvgRating(data?.avg_rating ?? null);
          setReviewCount(data?.review_count ?? 0);
        } else {
          const data = await reviewsApi.mine();
          const safeReviews = Array.isArray(data) ? data : [];
          setReviews(safeReviews);
          setReviewCount(safeReviews.length);
        }
      } catch (e) {
        setError(e.message);
      } finally {
        setLoading(false);
      }
    };

    fetchReviews();
  }, [user]);

  const isSeller = user?.role === 'seller';

  return (
    <AppShell>
      {/* Hero banner */}
      <div style={{
        background: 'linear-gradient(135deg,#f0faf4 0%,#d4edda 50%,#a8d5b5 100%)',
        borderRadius: 'var(--radius-lg)',
        padding: '32px 40px',
        marginBottom: 32,
        position: 'relative',
        overflow: 'hidden',
      }}>
        {/* Decorative hills */}
        <div style={{
          position: 'absolute', bottom: 0, left: 0, right: 0,
          height: 40, background: 'linear-gradient(180deg,#6ab04c 0%,#2d8e60 100%)',
          borderRadius: '50% 50% 0 0 / 30% 30% 0 0',
        }} />
        <div style={{ position: 'relative', zIndex: 1 }}>
          <h1 style={{ fontFamily: 'var(--font-display)', fontWeight: 800, fontSize: 28 }}>
            ⭐ {isSeller ? 'My Farm Reviews' : 'My Reviews'}
          </h1>
          {isSeller && avgRating !== null && (
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginTop: 8 }}>
              <StarRating value={Math.round(avgRating)} readOnly />
              <span style={{ fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 22 }}>
                {avgRating.toFixed(1)}
              </span>
              <span style={{ color: 'var(--gray-600)', fontSize: 14 }}>
                ({reviewCount} review{reviewCount !== 1 ? 's' : ''})
              </span>
            </div>
          )}
        </div>
      </div>

      {error && <div className="alert alert-error" style={{ marginBottom: 24 }}>{error}</div>}

      {/* Leave review notice (buyer only) */}
      {!isSeller && (
        <div className="alert alert-info" style={{ marginBottom: 24 }}>
          💡 You can leave a review on any <strong>completed</strong> order from the My Orders page.
        </div>
      )}

      {loading ? (
        <div className="spinner" />
      ) : reviews.length === 0 ? (
        <div style={{ textAlign: 'center', padding: 56, color: 'var(--gray-600)' }}>
          <div style={{ fontSize: 48, marginBottom: 12 }}>⭐</div>
          <p>{isSeller ? 'No reviews received yet.' : 'You haven\'t left any reviews yet.'}</p>
        </div>
      ) : (
        <>
          <div style={{ fontFamily: 'var(--font-display)', fontWeight: 700, fontSize: 13, color: 'var(--gray-600)', letterSpacing: 1, textTransform: 'uppercase', marginBottom: 12 }}>
            Latest Reviews
          </div>
          {reviews.map(r => (
            <ReviewCard key={r.id} review={r} isSeller={isSeller} />
          ))}
        </>
      )}
    </AppShell>
  );
}
